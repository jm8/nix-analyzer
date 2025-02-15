use std::{fmt, sync::Arc};

use lazy_regex::regex;
use rnix::{
    ast::{Expr, InheritFrom},
    TextRange, TextSize,
};
use ropey::Rope;
use rowan::ast::AstNode;
use tokio::sync::Mutex;
use tower_lsp::lsp_types::{CompletionItem, CompletionTextEdit, Position, Range, TextEdit};

use crate::{
    evaluator::{Evaluator, GetAttributesRequest},
    syntax::{
        get_variables, in_context, in_context_with_select, locate_cursor, parse, with_expression,
        LocationWithinExpr,
    },
};

pub async fn complete(
    source: &str,
    offset: u32,
    evaluator: Arc<Mutex<Evaluator>>,
) -> Option<Vec<CompletionItem>> {
    let strategy = get_completion_strategy(source, offset)?;

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
fn get_completion_strategy(source: &str, offset: u32) -> Option<CompletionStrategy> {
    let mut source = source.to_string();
    source.insert_str(offset as usize, "aaa");
    let root = parse(&source);
    let location = locate_cursor(&root, offset)?;
    eprintln!("Completing at {:?}", location);

    let rope = Rope::from_str(&source);
    let text_range = location.token.text_range();
    let text_range = TextRange::new(text_range.start(), text_range.end() - TextSize::new(3)); // aaa
    let range = rope_text_range_to_range(&rope, text_range);
    match location.location_within {
        LocationWithinExpr::Normal => {
            let attrs_expression = with_expression(&location.expr);
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
                attrs_expression: Some(in_context(&inherit_from.expr()?)),
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

                let attrs_expression =
                    Some(in_context_with_select(&attrs, attrpath.attrs().take(index)));

                Some(CompletionStrategy {
                    attrs_expression,
                    range,
                    variables: vec![],
                })
            }
            Expr::AttrSet(_attr_set) => None,
            _ => None,
        },
        LocationWithinExpr::PatEntry => None,
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

fn escape_attr(attr: &str) -> String {
    let re = regex!("^[A-Za-z_][A-Za-z_0-9'-]*$");
    if re.is_match(attr) {
        attr.to_string()
    } else {
        escape_string(attr)
    }
}

pub fn escape_string(text: &str) -> String {
    format!("\"{}\"", EscapeStringFragment(text))
}

#[derive(Debug, Clone)]
pub struct EscapeStringFragment<'a>(pub &'a str);

impl fmt::Display for EscapeStringFragment<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        for (i, ch) in self.0.char_indices() {
            match ch {
                '"' => "\\\"",
                '\\' => "\\\\",
                '\n' => "\\n",
                '\r' => "\\r",
                '\t' => "\\r",
                '$' if self.0[i..].starts_with("${") => "\\$",
                _ => {
                    ch.fmt(f)?;
                    continue;
                }
            }
            .fmt(f)?;
        }
        Ok(())
    }
}

#[cfg(test)]
mod test {
    use std::sync::Arc;

    use expect_test::{expect, Expect};
    use itertools::Itertools;
    use tokio::sync::Mutex;

    use crate::evaluator::Evaluator;

    use super::complete;

    async fn check_complete(source: &str, expected: Expect) {
        let (left, right) = source.split("$0").collect_tuple().unwrap();
        let offset = left.len() as u32;

        let evaluator = Arc::new(Mutex::new(Evaluator::new()));

        let source = format!("{}{}", left, right);
        let actual = complete(&source, offset, evaluator)
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
                    "b",
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
    async fn test_complete_with_of_let() {
        check_complete(
            r#"let x = { one = 1; two = 2; }; in with x; $0"#,
            expect![[r#"
                [
                    "one",
                    "two",
                    "x",
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
    async fn test_complete_rec_attrs() {
        check_complete(
            r#"rec { a = $0; b = 2; }"#,
            expect![[r#"
                [
                    "a",
                    "b",
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
}
