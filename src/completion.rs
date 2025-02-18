use std::{fmt, sync::Arc};

use itertools::Itertools;
use lazy_regex::regex;
use rnix::{
    ast::{Expr, Param},
    TextRange, TextSize,
};
use ropey::Rope;
use tokio::sync::Mutex;
use tower_lsp::lsp_types::{CompletionItem, CompletionTextEdit, Position, Range, TextEdit};

use crate::{
    evaluator::{Evaluator, GetAttributesRequest},
    file_types::FileInfo,
    lambda_arg::get_lambda_arg,
    schema::get_schema,
    syntax::{
        escape_attr, get_variables, in_context, in_context_with_select, locate_cursor, parse,
        with_expression, LocationWithinExpr,
    },
};

pub async fn complete(
    source: &str,
    file_info: &FileInfo,
    offset: u32,
    evaluator: Arc<Mutex<Evaluator>>,
) -> Option<Vec<CompletionItem>> {
    let strategy = get_completion_strategy(source, file_info, offset)?;

    let attr_completions = match strategy.attrs_expression {
        Some(expression) => evaluator
            .lock()
            .await
            .get_attributes(&GetAttributesRequest { expression })
            .await
            .ok()
            .unwrap_or_default(),
        None => vec![],
    };

    Some(
        attr_completions
            .iter()
            .chain(strategy.variables.iter())
            .sorted()
            .map(|completion| CompletionItem {
                label: completion.clone(),
                text_edit: Some(CompletionTextEdit::Edit(TextEdit {
                    range: strategy.range,
                    new_text: escape_attr(completion),
                })),
                commit_characters: Some(vec![".".to_string()]),
                ..Default::default()
            })
            .collect(),
    )
}

struct CompletionStrategy {
    range: Range,
    attrs_expression: Option<String>,
    variables: Vec<String>,
}

// This is a separate function to enforce that it is synchronus, because syntax trees are not Send
fn get_completion_strategy(
    source: &str,
    file_info: &FileInfo,
    offset: u32,
) -> Option<CompletionStrategy> {
    let mut source = source.to_string();
    source.insert_str(offset as usize, "aaa");
    let root = parse(&source);
    let location = locate_cursor(&root, offset)?;
    eprintln!("Completing at {:?}", location);

    let rope = Rope::from_str(&source);
    let text_range = location.token.text_range();
    let text_range = TextRange::new(text_range.start(), text_range.end() - TextSize::new(3)); // aaa
    let range = rope_text_range_to_range(&rope, text_range);

    let schema = get_schema(&location.expr, file_info);

    match location.location_within {
        LocationWithinExpr::Normal => {
            let attrs_expression = with_expression(&location.expr, file_info);
            let variables = get_variables(&location.expr);
            Some(CompletionStrategy {
                range,
                attrs_expression,
                variables,
            })
        }
        LocationWithinExpr::Inherit(inherit) => Some(match inherit.from() {
            Some(inherit_from) => CompletionStrategy {
                range,
                attrs_expression: Some(in_context(&inherit_from.expr()?, file_info)),
                variables: vec![],
            },
            None => CompletionStrategy {
                range,
                attrs_expression: None,
                variables: get_variables(&location.expr),
            },
        }),
        LocationWithinExpr::Attrpath(attrpath, index) => match location.expr {
            Expr::Select(select) => {
                let attrs = select.expr().unwrap();

                let attrs_expression = Some(in_context_with_select(
                    &attrs,
                    attrpath.attrs().take(index),
                    file_info,
                ));

                Some(CompletionStrategy {
                    attrs_expression,
                    range,
                    variables: vec![],
                })
            }
            Expr::AttrSet(_) => {
                let mut schema = schema;
                for attr in attrpath.attrs().take(index) {
                    schema = schema.attr_subschema(&attr).clone();
                }

                Some(CompletionStrategy {
                    attrs_expression: None,
                    range,
                    variables: schema.properties(),
                })
            }
            _ => None,
        },
        LocationWithinExpr::PatEntry => {
            let lambda = match location.expr {
                Expr::Lambda(lambda) => lambda,
                _ => unreachable!("shouldn't happen"),
            };
            if lambda.token_colon().is_none()
                && lambda.param().is_some_and(|param| match param {
                    Param::Pattern(pattern) => pattern.pat_entries().count() == 1,
                    Param::IdentParam(_) => false,
                })
            {
                // rnix parses {} as a lambda, but it should actually be considered an attrset, and use schema completion
                return Some(CompletionStrategy {
                    attrs_expression: None,
                    range,
                    variables: schema.properties(),
                });
            }
            let lambda_arg = get_lambda_arg(&lambda, file_info);
            Some(CompletionStrategy {
                range,
                attrs_expression: Some(lambda_arg),
                variables: vec![],
            })
        }
        LocationWithinExpr::PatBind => None,
    }
}

fn rope_offset_to_position(rope: &Rope, offset: impl Into<usize>) -> Position {
    let offset = Into::<usize>::into(offset);
    let line = rope.byte_to_line(offset) as u32;
    let character = (offset - rope.line_to_byte(line as usize)) as u32;
    Position { line, character }
}

fn rope_text_range_to_range(rope: &Rope, text_range: TextRange) -> Range {
    let start = rope_offset_to_position(rope, text_range.start());
    let end = rope_offset_to_position(rope, text_range.end());
    Range { start, end }
}
#[cfg(test)]
mod test {
    use std::sync::Arc;

    use expect_test::{expect, Expect};
    use itertools::Itertools;
    use tokio::sync::Mutex;

    use crate::{
        evaluator::Evaluator,
        file_types::{FileInfo, FileType},
        flakes::get_flake_filetype,
    };

    use super::complete;

    async fn check_complete_with_filetype(source: &str, expected: Expect, file_type: &FileType) {
        let (left, right) = source.split("$0").collect_tuple().unwrap();
        let offset = left.len() as u32;

        let evaluator = Arc::new(Mutex::new(Evaluator::new()));

        let source = format!("{}{}", left, right);
        let actual = complete(
            &source,
            &FileInfo {
                file_type: file_type.clone(),
                path: "/test/path".into(),
            },
            offset,
            evaluator,
        )
        .await
        .unwrap()
        .iter()
        .map(|item| item.label.clone())
        .collect_vec();

        expected.assert_debug_eq(&actual);
    }

    async fn check_complete(source: &str, expected: Expect) {
        check_complete_with_filetype(
            source,
            expected,
            &FileType::Custom {
                lambda_arg: "{}".to_string(),
                schema: "{}".to_string(),
            },
        )
        .await;
    }

    async fn check_complete_flake(source: &str, old_lock_file: Option<&str>, expected: Expect) {
        let (left, right) = source.split("$0").collect_tuple().unwrap();
        let offset = left.len() as u32;

        let evaluator = Arc::new(Mutex::new(Evaluator::new()));

        let file_type = get_flake_filetype(evaluator.clone(), source, old_lock_file)
            .await
            .unwrap();

        let source = format!("{}{}", left, right);
        let actual = complete(
            &source,
            &FileInfo {
                file_type: file_type.clone(),
                path: "/test/path".into(),
            },
            offset,
            evaluator,
        )
        .await
        .unwrap()
        .iter()
        .map(|item| item.label.clone())
        .collect_vec();

        expected.assert_debug_eq(&actual);
    }

    #[test_log::test(tokio::test)]
    async fn test_complete_let_in() {
        check_complete(
            r#"let x = {a = 2; "your mom" = 3;}; in x.$0b"#,
            expect![[r#"
                [
                    "a",
                    "your mom",
                ]
            "#]],
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_complete_select_multi_level() {
        check_complete(
            r#"let x = {a = { b = 1; }; }; in x.a.$0"#,
            expect![[r#"
                [
                    "b",
                ]
            "#]],
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_complete_derivation() {
        check_complete(
            r#"(derivation { name = "myname"; builder = "mybuilder"; system = "mysystem"; }).$0"#,
            expect![[r#"
                [
                    "all",
                    "builder",
                    "drvAttrs",
                    "drvPath",
                    "name",
                    "out",
                    "outPath",
                    "outputName",
                    "system",
                    "type",
                ]
            "#]],
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_complete_nixpkgs() {
        check_complete(
            &format!("(import {} {{}}).hello.$0", env!("nixpkgs")),
            expect![[r#"
                [
                    "__ignoreNulls",
                    "__structuredAttrs",
                    "all",
                    "args",
                    "buildInputs",
                    "builder",
                    "cmakeFlags",
                    "configureFlags",
                    "depsBuildBuild",
                    "depsBuildBuildPropagated",
                    "depsBuildTarget",
                    "depsBuildTargetPropagated",
                    "depsHostHost",
                    "depsHostHostPropagated",
                    "depsTargetTarget",
                    "depsTargetTargetPropagated",
                    "doCheck",
                    "doInstallCheck",
                    "drvAttrs",
                    "drvPath",
                    "inputDerivation",
                    "mesonFlags",
                    "meta",
                    "name",
                    "nativeBuildInputs",
                    "out",
                    "outPath",
                    "outputName",
                    "outputs",
                    "override",
                    "overrideAttrs",
                    "overrideDerivation",
                    "passthru",
                    "patches",
                    "pname",
                    "postInstallCheck",
                    "propagatedBuildInputs",
                    "propagatedNativeBuildInputs",
                    "src",
                    "stdenv",
                    "strictDeps",
                    "system",
                    "tests",
                    "type",
                    "userHook",
                    "version",
                ]
            "#]],
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_complete_builtins() {
        check_complete(
            "builtins.$0",
            expect![[r#"
                [
                    "abort",
                    "add",
                    "addDrvOutputDependencies",
                    "addErrorContext",
                    "all",
                    "any",
                    "appendContext",
                    "attrNames",
                    "attrValues",
                    "baseNameOf",
                    "bitAnd",
                    "bitOr",
                    "bitXor",
                    "break",
                    "builtins",
                    "catAttrs",
                    "ceil",
                    "compareVersions",
                    "concatLists",
                    "concatMap",
                    "concatStringsSep",
                    "convertHash",
                    "currentSystem",
                    "currentTime",
                    "deepSeq",
                    "derivation",
                    "derivationStrict",
                    "dirOf",
                    "div",
                    "elem",
                    "elemAt",
                    "false",
                    "fetchGit",
                    "fetchMercurial",
                    "fetchTarball",
                    "fetchTree",
                    "fetchurl",
                    "filter",
                    "filterSource",
                    "findFile",
                    "flakeRefToString",
                    "floor",
                    "foldl'",
                    "fromJSON",
                    "fromTOML",
                    "functionArgs",
                    "genList",
                    "genericClosure",
                    "getAttr",
                    "getContext",
                    "getEnv",
                    "groupBy",
                    "hasAttr",
                    "hasContext",
                    "hashFile",
                    "hashString",
                    "head",
                    "import",
                    "intersectAttrs",
                    "isAttrs",
                    "isBool",
                    "isFloat",
                    "isFunction",
                    "isInt",
                    "isList",
                    "isNull",
                    "isPath",
                    "isString",
                    "langVersion",
                    "length",
                    "lessThan",
                    "listToAttrs",
                    "map",
                    "mapAttrs",
                    "match",
                    "mul",
                    "nixPath",
                    "nixVersion",
                    "null",
                    "parseDrvName",
                    "parseFlakeRef",
                    "partition",
                    "path",
                    "pathExists",
                    "placeholder",
                    "readDir",
                    "readFile",
                    "readFileType",
                    "removeAttrs",
                    "replaceStrings",
                    "scopedImport",
                    "seq",
                    "sort",
                    "split",
                    "splitVersion",
                    "storeDir",
                    "storePath",
                    "stringLength",
                    "sub",
                    "substring",
                    "tail",
                    "throw",
                    "toFile",
                    "toJSON",
                    "toPath",
                    "toString",
                    "toXML",
                    "trace",
                    "traceVerbose",
                    "true",
                    "tryEval",
                    "typeOf",
                    "unsafeDiscardOutputDependency",
                    "unsafeDiscardStringContext",
                    "unsafeGetAttrPos",
                    "warn",
                    "zipAttrsWith",
                ]
            "#]],
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_complete_weird() {
        check_complete(
            r#"{ "Don\"t mind my Weird String" = 1; abc = 2; }.$0"#,
            expect![[r#"
                [
                    "Don\"t mind my Weird String",
                    "abc",
                ]
            "#]],
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_complete_variable() {
        check_complete(
            r#"$0"#,
            expect![[r#"
                [
                    "abort",
                    "baseNameOf",
                    "break",
                    "builtins",
                    "derivation",
                    "derivationStrict",
                    "dirOf",
                    "false",
                    "fetchGit",
                    "fetchMercurial",
                    "fetchTarball",
                    "fetchTree",
                    "fromTOML",
                    "import",
                    "isNull",
                    "map",
                    "null",
                    "placeholder",
                    "removeAttrs",
                    "scopedImport",
                    "throw",
                    "toString",
                    "true",
                ]
            "#]],
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_complete_with() {
        check_complete(
            r#"with {a = 2; b = 3;}; $0"#,
            expect![[r#"
                [
                    "a",
                    "abort",
                    "b",
                    "baseNameOf",
                    "break",
                    "builtins",
                    "derivation",
                    "derivationStrict",
                    "dirOf",
                    "false",
                    "fetchGit",
                    "fetchMercurial",
                    "fetchTarball",
                    "fetchTree",
                    "fromTOML",
                    "import",
                    "isNull",
                    "map",
                    "null",
                    "placeholder",
                    "removeAttrs",
                    "scopedImport",
                    "throw",
                    "toString",
                    "true",
                ]
            "#]],
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_complete_with_of_let() {
        check_complete(
            r#"let x = { one = 1; two = 2; }; in with x; $0"#,
            expect![[r#"
                [
                    "abort",
                    "baseNameOf",
                    "break",
                    "builtins",
                    "derivation",
                    "derivationStrict",
                    "dirOf",
                    "false",
                    "fetchGit",
                    "fetchMercurial",
                    "fetchTarball",
                    "fetchTree",
                    "fromTOML",
                    "import",
                    "isNull",
                    "map",
                    "null",
                    "one",
                    "placeholder",
                    "removeAttrs",
                    "scopedImport",
                    "throw",
                    "toString",
                    "true",
                    "two",
                    "x",
                ]
            "#]],
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_complete_rec_attrs() {
        check_complete(
            r#"rec { a = $0; b = 2; }"#,
            expect![[r#"
                [
                    "a",
                    "abort",
                    "b",
                    "baseNameOf",
                    "break",
                    "builtins",
                    "derivation",
                    "derivationStrict",
                    "dirOf",
                    "false",
                    "fetchGit",
                    "fetchMercurial",
                    "fetchTarball",
                    "fetchTree",
                    "fromTOML",
                    "import",
                    "isNull",
                    "map",
                    "null",
                    "placeholder",
                    "removeAttrs",
                    "scopedImport",
                    "throw",
                    "toString",
                    "true",
                ]
            "#]],
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_complete_inherit() {
        check_complete(
            r#"let aaa = 2; in { inherit $0; } "#,
            expect![[r#"
            [
                "aaa",
                "abort",
                "baseNameOf",
                "break",
                "builtins",
                "derivation",
                "derivationStrict",
                "dirOf",
                "false",
                "fetchGit",
                "fetchMercurial",
                "fetchTarball",
                "fetchTree",
                "fromTOML",
                "import",
                "isNull",
                "map",
                "null",
                "placeholder",
                "removeAttrs",
                "scopedImport",
                "throw",
                "toString",
                "true",
            ]
        "#]],
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_complete_inherit_from() {
        check_complete(
            r#"let a = { b = 3; }; in { inherit (a) $0; } "#,
            expect![[r#"
                [
                    "b",
                ]
            "#]],
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_complete_lambda_params() {
        check_complete(
            r#"{ a, b } @ c: $0"#,
            expect![[r#"
                [
                    "a",
                    "abort",
                    "b",
                    "baseNameOf",
                    "break",
                    "builtins",
                    "c",
                    "derivation",
                    "derivationStrict",
                    "dirOf",
                    "false",
                    "fetchGit",
                    "fetchMercurial",
                    "fetchTarball",
                    "fetchTree",
                    "fromTOML",
                    "import",
                    "isNull",
                    "map",
                    "null",
                    "placeholder",
                    "removeAttrs",
                    "scopedImport",
                    "throw",
                    "toString",
                    "true",
                ]
            "#]],
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_complete_lambda_arg() {
        check_complete_with_filetype(
            r#"x: x.$0"#,
            expect![[r#"
                [
                    "test1",
                    "test2",
                ]
            "#]],
            &FileType::Custom {
                lambda_arg: "{test1 = 1; test2 = 1;}".to_string(),
                schema: "{}".to_string(),
            },
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_complete_lambda_arg_inside() {
        check_complete_with_filetype(
            r#"{aaaa, $0}: "#,
            expect![[r#"
                [
                    "test1",
                    "test2",
                ]
            "#]],
            &FileType::Custom {
                lambda_arg: "{test1 = 1; test2 = 1;}".to_string(),
                schema: "{}".to_string(),
            },
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_complete_schema() {
        check_complete_with_filetype(
            r#"{ a = 1; $0 }"#,
            expect![[r#"
                [
                    "abc",
                    "xyz",
                ]
            "#]],
            &FileType::Custom {
                lambda_arg: "{}".to_string(),
                schema: r#"{ "properties": {"abc": {"properties": {"one": {}}}, "xyz": {"properties": {"two": {}}}} }"#.to_string(),
            },
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_complete_empty() {
        check_complete_with_filetype(
            r#"{ $0 }"#,
            expect![[r#"
                [
                    "abc",
                    "xyz",
                ]
            "#]],
            &FileType::Custom {
                lambda_arg: "{}".to_string(),
                schema: r#"{ "properties": {"abc": {"properties": {"one": {}}}, "xyz": {"properties": {"two": {}}}} }"#.to_string(),
            },
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_complete_schema_sub() {
        check_complete_with_filetype(
            r#"{ a = 1; abc.$0 }"#,
            expect![[r#"
                [
                    "one",
                ]
            "#]],
            &FileType::Custom {
                lambda_arg: "{}".to_string(),
                schema: r#"{ "properties": {"abc": {"properties": {"one": {}}}, "xyz": {"properties": {"two": {}}}} }"#.to_string(),
            },
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_complete_schema_nested() {
        check_complete_with_filetype(
            r#"{ a = 1; abc = { x = 1; $0 }; }"#,
            expect![[r#"
                [
                    "one",
                ]
            "#]],
            &FileType::Custom {
                lambda_arg: "{}".to_string(),
                schema: r#"{ "properties": {"abc": {"properties": {"one": {}}}, "xyz": {"properties": {"two": {}}}} }"#.to_string(),
            },
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_complete_schema_() {
        check_complete_with_filetype(
            r#"{ a = 1; $0 }"#,
            expect![[r#"
                [
                    "appstream",
                    "assertions",
                    "boot",
                    "console",
                    "containers",
                    "docker-containers",
                    "documentation",
                    "dysnomia",
                    "ec2",
                    "environment",
                    "fileSystems",
                    "fonts",
                    "gtk",
                    "hardware",
                    "i18n",
                    "ids",
                    "isSpecialisation",
                    "jobs",
                    "krb5",
                    "lib",
                    "location",
                    "meta",
                    "nesting",
                    "networking",
                    "nix",
                    "nixops",
                    "nixpkgs",
                    "oci",
                    "openstack",
                    "passthru",
                    "power",
                    "powerManagement",
                    "programs",
                    "qt",
                    "qt5",
                    "security",
                    "services",
                    "snapraid",
                    "sound",
                    "specialisation",
                    "stubby",
                    "swapDevices",
                    "system",
                    "systemd",
                    "time",
                    "users",
                    "virtualisation",
                    "warnings",
                    "xdg",
                    "zramSwap",
                ]
            "#]],
            &FileType::Custom {
                lambda_arg: "{}".to_string(),
                schema: include_str!("./nixos_module_schema.json").to_string(),
            },
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_complete_rec_value() {
        check_complete(
            r#"let a = {c = 1;}; in rec { a = { b = 1; }; x = a.$0 }"#,
            expect![[r#"
                [
                    "b",
                ]
            "#]],
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_complete_non_rec_value() {
        check_complete(
            r#"let a = {c = 1;}; in { a = { b = 1; }; x = a.$0 }"#,
            expect![[r#"
                [
                    "c",
                ]
            "#]],
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_complete_flake_inputs() {
        check_complete_flake(
            r#"
            {
              inputs.flake-utils.url = "github:numtide/flake-utils?rev=919d646de7be200f3bf08cb76ae1f09402b6f9b4";
              outputs = {flake-utils}: flake-utils.lib.$0
            }
            "#,
            None,
            expect![[r#"
                [
                    "c",
                ]
            "#]],
        )
        .await;
    }
}
