use anyhow::{anyhow, Result};
use lsp_types::Range;
use rnix::{ast::Expr, TextRange};
use ropey::Rope;
use rowan::ast::AstNode;
use std::{ops::Not, path::PathBuf};

use crate::{
    evaluator::{proto::HoverRequest, Evaluator},
    file_types::FileInfo,
    safe_stringification::safe_stringify_attr,
    schema::get_schema,
    syntax::{
        ancestor_exprs, ancestor_exprs_inclusive, find_variable, in_context,
        in_context_custom, in_context_with_select, locate_cursor, parse, rope_text_range_to_range, FoundVariable, LocationWithinExpr,
    },
};

#[derive(Debug)]
enum HoverOrigin {
    Param(TextRange),
    Binding(TextRange),
    With(TextRange),
    Select,
    Builtin,
}

#[derive(Debug)]
struct HoverStrategy {
    range: Range,
    expression: Option<String>,
    attr: Option<String>,
    origin: HoverOrigin,
}

const DOCROOT: &str = "https://nix.dev/manual/nix/2.25";

pub async fn hover(
    source: &str,
    file_info: &FileInfo,
    offset: u32,
    evaluator: &mut Evaluator,
) -> Result<HoverResult> {
    let strategy =
        get_hover_strategy(source, file_info, offset).ok_or(anyhow!("can't hover this"))?;

    let expression = strategy.expression.ok_or(anyhow!("no expression"))?;

    let result = evaluator
        .hover(&HoverRequest {
            expression,
            attr: strategy.attr,
        })
        .await?;

    let md = if result.r#type == "primop" {
        // TODO: make better
        result
            .value
            .replace("@docroot@", DOCROOT)
            .replace(".md", "")
    } else {
        format!("### {}\n\n```nix\n{}\n```", result.r#type, result.value)
    };

    Ok(HoverResult {
        md,
        position: result.path.is_empty().not().then_some(Position {
            line: result.row as u32,
            col: result.col as u32,
            path: result.path.into(),
        }),
    })
}

fn get_hover_strategy(source: &str, file_info: &FileInfo, offset: u32) -> Option<HoverStrategy> {
    let root = parse(source);
    let location = locate_cursor(&root, offset)?;

    let rope = Rope::from_str(source);
    let text_range = location.token.text_range();
    let text_range = TextRange::new(text_range.start(), text_range.end());
    let range = rope_text_range_to_range(&rope, text_range);

    let schema = get_schema(&location.expr, file_info);

    fn hover_variable(
        name: &str,
        expr: &Expr,
        range: Range,
        file_info: &FileInfo,
    ) -> Option<HoverStrategy> {
        if let Some(found) = find_variable(expr, name) {
            Some(HoverStrategy {
                range,
                expression: Some(in_context_custom(
                    name,
                    ancestor_exprs_inclusive(expr),
                    file_info,
                )),
                attr: None,
                origin: match found {
                    FoundVariable::PatBind(pat_bind) => {
                        HoverOrigin::Param(pat_bind.syntax().text_range())
                    }
                    FoundVariable::PatEntry(pat_entry) => {
                        HoverOrigin::Param(pat_entry.syntax().text_range())
                    }
                    FoundVariable::IdentParam(ident_param) => {
                        HoverOrigin::Param(ident_param.syntax().text_range())
                    }
                    FoundVariable::AttrpathValue(attrpath_value) => {
                        HoverOrigin::Binding(attrpath_value.syntax().text_range())
                    }
                    FoundVariable::Inherit(inherit) => {
                        HoverOrigin::Binding(inherit.syntax().text_range())
                    }
                    FoundVariable::Builtin => HoverOrigin::Builtin,
                },
            })
        } else {
            let with = ancestor_exprs(expr).find_map(|ancestor| match ancestor {
                Expr::With(with) => Some(with),
                _ => None,
            });

            if let Some(with) = with
                && let Some(namespace) = with.namespace()
            {
                Some(HoverStrategy {
                    range,
                    expression: Some(in_context(&namespace, file_info)),
                    attr: Some(name.to_string()),
                    origin: HoverOrigin::With(with.syntax().text_range()),
                })
            } else {
                None
            }
        }
    }
    match location.location_within {
        LocationWithinExpr::Normal => hover_variable(
            &location.token.to_string(),
            &location.expr,
            range,
            file_info,
        ),
        LocationWithinExpr::Inherit(inherit, index) => match inherit.from() {
            Some(inherit_from) => Some(HoverStrategy {
                range,
                expression: Some(in_context(&inherit_from.expr()?, file_info)),
                attr: Some(safe_stringify_attr(
                    &inherit.attrs().nth(index).unwrap(),
                    file_info.base_path(),
                )),
                origin: HoverOrigin::Binding(inherit.syntax().text_range()),
            }),
            None => hover_variable(
                &location.token.to_string(),
                &location.expr,
                range,
                file_info,
            ),
        },
        LocationWithinExpr::Attrpath(attrpath, index) => match location.expr {
            Expr::Select(select) => {
                let attrs = select.expr().unwrap();

                let expression = Some(in_context_with_select(
                    &attrs,
                    attrpath.attrs().take(index),
                    file_info,
                ));

                Some(HoverStrategy {
                    range,
                    expression,
                    attr: Some(safe_stringify_attr(
                        &attrpath.attrs().nth(index).unwrap(),
                        file_info.base_path(),
                    )),
                    origin: HoverOrigin::Select,
                })
            }
            Expr::AttrSet(_) => {
                let mut schema = schema;
                for attr in attrpath.attrs().take(index) {
                    schema = schema.attr_subschema(&attr).clone();
                }

                // Some(HoverStrategy {
                //     attrs_expression: None,
                //     range,
                //     variables: schema.properties(),
                // })
                None
            }
            _ => None,
        },
        // LocationWithinExpr::PatEntry => {
        //     let lambda = match location.expr {
        //         Expr::Lambda(lambda) => lambda,
        //         _ => unreachable!("shouldn't happen"),
        //     };
        //     if lambda.token_colon().is_none()
        //         && lambda.param().is_some_and(|param| match param {
        //             Param::Pattern(pattern) => pattern.pat_entries().count() == 1,
        //             Param::IdentParam(_) => false,
        //         })
        //     {
        //         // rnix parses {} as a lambda, but it should actually be considered an attrset, and use schema completion
        //         return Some(CompletionStrategy {
        //             attrs_expression: None,
        //             range,
        //             variables: schema.properties(),
        //         });
        //     }
        //     let lambda_arg = get_lambda_arg(&lambda, file_info);
        //     Some(CompletionStrategy {
        //         range,
        //         attrs_expression: Some(lambda_arg),
        //         variables: vec![],
        //     })
        // }
        // LocationWithinExpr::PatBind => None,
        _ => None,
    }
}

#[derive(Debug)]
pub struct Position {
    pub line: u32,
    pub col: u32,
    pub path: PathBuf,
}

pub struct HoverResult {
    pub md: String,
    pub position: Option<Position>,
}

#[cfg(test)]
mod test {

    use expect_test::{expect, Expect};
    use itertools::Itertools;

    use crate::{
        evaluator::Evaluator,
        file_types::{FileInfo, FileType},
    };

    use super::hover;

    async fn check_hover_with_filetype(source: &str, expected: Expect, file_type: &FileType) {
        let (left, right) = source.split("$0").collect_tuple().unwrap();
        let offset = left.len() as u32;

        let mut evaluator = Evaluator::new().await;

        let source = format!("{}{}", left, right);
        let actual = hover(
            &source,
            &FileInfo {
                file_type: file_type.clone(),
                path: "/test/test.nix".into(),
            },
            offset,
            &mut evaluator,
        )
        .await
        .unwrap();

        let actual = match actual.position {
            Some(position) => format!(
                "{}:{}:{}\n\n{}",
                position.path.display(),
                position.line,
                position.col,
                actual.md
            ),
            None => format!("no position\n\n{}", actual.md),
        };

        expected.assert_eq(&actual);
    }

    async fn check_hover(source: &str, expected: Expect) {
        check_hover_with_filetype(
            source,
            expected,
            &FileType::Custom {
                lambda_arg: "{}".to_string(),
                schema: "{}".to_string(),
            },
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_hover_builtin() {
        check_hover(
            "imp$0ort",
            expect![[r#"
                no position

                ### built-in function `import` *`path `*


                Load, parse, and return the Nix expression in the file *path*.

                > **Note**
                >
                > Unlike some languages, `import` is a regular function in Nix.

                The *path* argument must meet the same criteria as an [interpolated expression](https://nix.dev/manual/nix/2.25/language/string-interpolation#interpolated-expression).

                If *path* is a directory, the file `default.nix` in that directory is used if it exists.

                > **Example**
                >
                > ```console
                > $ echo 123 > default.nix
                > ```
                >
                > Import `default.nix` from the current directory.
                >
                > ```nix
                > import ./.
                > ```
                >
                >     123

                Evaluation aborts if the file doesn’t exist or contains an invalid Nix expression.

                A Nix expression loaded by `import` must not contain any *free variables*, that is, identifiers that are not defined in the Nix expression itself and are not built-in.
                Therefore, it cannot refer to variables that are in scope at the call site.

                > **Example**
                >
                > If you have a calling expression
                >
                > ```nix
                > rec {
                >   x = 123;
                >   y = import ./foo.nix;
                > }
                > ```
                >
                >  then the following `foo.nix` will give an error:
                >
                >  ```nix
                >  # foo.nix
                >  x + 456
                >  ```
                >
                >  since `x` is not in scope in `foo.nix`.
                > If you want `x` to be available in `foo.nix`, pass it as a function argument:
                >
                >  ```nix
                >  rec {
                >    x = 123;
                >    y = import ./foo.nix x;
                >  }
                >  ```
                >
                >  and
                >
                >  ```nix
                >  # foo.nix
                >  x: x + 456
                >  ```
                >
                >  The function argument doesn’t have to be called `x` in `foo.nix`; any name would work.
            "#]],
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_hover_let() {
        check_hover(
            "let x = 5; in x$0",
            expect![[r#"
                no position

                ### integer

                ```nix
                5
                ```"#]],
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_hover_with() {
        check_hover(
            "let yourmother = 1; in with {aaa = 1; bbb = 2;}; aa$0a",
            expect![[r#"
                <<string>>:1:57

                ### attrset

                ```nix
                1
                ```"#]],
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_hover_inherit() {
        check_hover(
            r#"let aaa = "hello"; bbb = "world"; in { inherit aa$0a; }"#,
            expect![[r#"
                no position

                ### string

                ```nix
                "hello"
                ```"#]],
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_hover_inherit_from() {
        check_hover(
            r#"let x = {aaa = "hello"; bbb = "world";}; in { inherit (x) aa$0a; }"#,
            expect![[r#"
                <<string>>:1:12

                ### attrset

                ```nix
                "hello"
                ```"#]],
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_hover_select() {
        check_hover(
            r#"let x = {aaa.bbb.ccc = 2;}; in x.aaa.b$0bb.ccc"#,
            expect![[r#"
                <<string>>:1:12

                ### attrset

                ```nix
                {
                  ccc = 2;
                }
                ```"#]],
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_hover_select_with() {
        check_hover(
            r#"let x = {yyy.aaa.bbb.ccc = 2;}; in with x; yyy.a$0aa.bbb"#,
            expect![[r#"
                <<string>>:1:12

                ### attrset

                ```nix
                {
                  bbb = { /* ... */ };
                }
                ```"#]],
        )
        .await;
    }
}
