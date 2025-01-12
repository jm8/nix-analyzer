use std::sync::Arc;

use lazy_regex::regex;
use rnix::{
    ast::{Attrpath, Expr, Select},
    TextRange, TextSize,
};
use ropey::Rope;
use rowan::ast::AstNode;
use tokio::sync::Mutex;
use tower_lsp::lsp_types::{
    CompletionItem, CompletionTextEdit, Position, Range, TextEdit,
};

use crate::{
    evaluator::{Evaluator, GetAttributesRequest},
    syntax::{in_context_with_select, locate_cursor, parse, LocationWithinExpr},
};

pub async fn complete(
    source: &str,
    offset: u32,
    evaluator: Arc<Mutex<Evaluator>>,
) -> Option<Vec<CompletionItem>> {
    let mut source = source.to_string();
    source.insert_str(offset as usize, "aaa");
    let root = parse(&source);
    let location = locate_cursor(&root, offset)?;
    eprintln!("Completing at {:?}", location);
    match location.location_within {
        LocationWithinExpr::Normal => None,
        LocationWithinExpr::Inherit(_) => None,
        LocationWithinExpr::Attrpath(attrpath, index) => match location.expr {
            Expr::Select(select) => {
                complete_select(&select, &source, &attrpath, index, evaluator).await
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
        format!("\"{}\"", attr)
    }
}

async fn complete_select(
    select: &Select,
    source: &str,
    attrpath: &Attrpath,
    index: usize,
    evaluator: Arc<Mutex<Evaluator>>,
) -> Option<Vec<CompletionItem>> {
    let rope = Rope::from_str(source);
    let attr = attrpath.attrs().nth(index).unwrap();
    let text_range = attr.syntax().text_range();
    let text_range = TextRange::new(text_range.start(), text_range.end() - TextSize::new(3)); // aaa
    let range = rope_text_range_to_range(&rope, text_range);

    let attrs = select.expr()?;

    let expression = in_context_with_select(&attrs, attrpath.attrs().take(index));

    eprintln!("expression: {}", &expression);

    let completions = evaluator
        .lock()
        .await
        .get_attributes(&GetAttributesRequest { expression })
        .await
        .ok()?;
    Some(
        completions
            .iter()
            .map(|completion| CompletionItem {
                label: completion.clone(),
                text_edit: Some(CompletionTextEdit::Edit(TextEdit {
                    range,
                    new_text: escape_attr(completion),
                })),
                commit_characters: Some(vec![".".to_string()]),
                ..Default::default()
            })
            .collect(),
    )
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

    #[tokio::test]
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

    #[tokio::test]
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

    #[tokio::test]
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

    #[tokio::test]
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

    #[tokio::test]
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
}
