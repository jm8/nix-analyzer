use anyhow::{anyhow, Result};
use lsp_types::Range;
use rnix::{ast::Expr, TextRange};
use ropey::Rope;
use rowan::ast::AstNode;
use std::{path::PathBuf, sync::Arc};

use crate::{
    evaluator::{proto::HoverRequest, Evaluator},
    file_types::FileInfo,
    safe_stringification::safe_stringify_attr,
    schema::{get_schema, Schema},
    syntax::{
        ancestor_exprs, ancestor_exprs_inclusive, find_variable, in_context, in_context_custom,
        in_context_with_select, locate_cursor, parse, rope_text_range_to_range, FoundVariable,
        LocationWithinExpr,
    },
};

#[derive(Debug)]
enum HoverOrigin {
    Param(TextRange),
    Binding(TextRange),
    With(TextRange),
    Select,
    Builtin,
    Schema(Arc<Schema>),
    Path(PathBuf),
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

    if let HoverOrigin::Schema(schema) = strategy.origin {
        let md = schema
            .description()
            .ok_or(anyhow!("no description"))?
            .to_string();
        return Ok(HoverResult { md, position: None });
    }
    if let HoverOrigin::Path(path) = strategy.origin {
        let md = format!("```\n{}\n```", path.display());
        let default_nix_path = path.join("default.nix");
        let goto_path = if path.is_dir() && default_nix_path.exists() {
            default_nix_path
        } else {
            path
        };
        let position = Some(Position {
            line: 1,
            col: 1,
            path: goto_path,
        });
        return Ok(HoverResult { md, position });
    }

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

    let rope = Rope::from_str(source);

    let origin_range = match strategy.origin {
        HoverOrigin::Param(text_range) => Some(rope_text_range_to_range(&rope, text_range)),
        HoverOrigin::Binding(text_range) => Some(rope_text_range_to_range(&rope, text_range)),
        HoverOrigin::With(_) => None,
        HoverOrigin::Select => None,
        HoverOrigin::Builtin => None,
        HoverOrigin::Schema(_) => None,
        HoverOrigin::Path(_) => None,
    };

    let position = if let Some(origin_range) = origin_range {
        Some(Position {
            path: file_info.path.clone(),
            line: origin_range.start.line,
            col: origin_range.start.character,
        })
    } else if !result.path.is_empty() && result.path != "<<string>>" {
        Some(Position {
            path: result.path.into(),
            line: (result.row - 1) as u32,
            col: (result.col - 1) as u32,
        })
    } else {
        None
    };

    Ok(HoverResult { md, position })
}

fn get_hover_strategy(source: &str, file_info: &FileInfo, offset: u32) -> Option<HoverStrategy> {
    let root = parse(source);
    let location = locate_cursor(&root, offset)?;

    let rope = Rope::from_str(source);
    let text_range = location.token.text_range();
    let text_range = TextRange::new(text_range.start(), text_range.end());
    let range = rope_text_range_to_range(&rope, text_range);

    if let Expr::Path(path) = location.expr {
        let path = PathBuf::from(path.to_string());
        let path = if path.is_absolute() {
            path
        } else {
            file_info
                .base_path()
                .join(path.strip_prefix("./").unwrap_or(&path))
        };
        return Some(HoverStrategy {
            range: range,
            expression: None,
            attr: None,
            origin: HoverOrigin::Path(path),
        });
    }

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
                for attr in attrpath.attrs().take(index + 1) {
                    schema = schema.attr_subschema(&attr).clone();
                }

                Some(HoverStrategy {
                    expression: None,
                    range,
                    attr: None,
                    origin: HoverOrigin::Schema(schema),
                })
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
    use indoc::indoc;
    use itertools::Itertools;

    use crate::{
        evaluator::Evaluator,
        file_types::{FileInfo, FileType},
        schema::HOME_MANAGER_SCHEMA,
        testing::{create_test_analyzer, parse_test_input},
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
        let input = parse_test_input(source);
        let mut analysis = create_test_analyzer(&input).await;
        let location = input.location.unwrap();
        let actual = analysis
            .hover(&location.path, location.line, location.col)
            .await
            .unwrap()
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

    async fn check_hover_nixpkgs(relative_path: &str, source: &str, expected: Expect) {
        let input = format!("## {}/{}\n\n{}", env!("NIXPKGS"), relative_path, source);
        check_hover(&input, expected).await;
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
                /nowhere.nix:0:4

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
                no position

                ### integer

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
                /nowhere.nix:0:4

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
                /nowhere.nix:0:46

                ### string

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
                no position

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
                no position

                ### attrset

                ```nix
                {
                  bbb = { /* ... */ };
                }
                ```"#]],
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_hover_schema() {
        check_hover_with_filetype(
            r#"{ programs.zsh.enableAutosug$0gestions = false;  }"#,
            expect![[r#"
                no position

                ### option `programs.zsh.enableAutosuggestions`
                Alias of {option}`programs.zsh.autosuggestion.enable`.

                *Type:* boolean
            "#]],
            &FileType::Other {
                nixpkgs_path: env!("NIXPKGS").to_string(),
                schema: HOME_MANAGER_SCHEMA.clone(),
            },
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_hover_lambda() {
        check_hover_with_filetype(
            r#"{ pkgs }: { programs.zsh.enableAutosug$0gestions = false;  }"#,
            expect![[r#"
                no position

                ### option `programs.zsh.enableAutosuggestions`
                Alias of {option}`programs.zsh.autosuggestion.enable`.

                *Type:* boolean
            "#]],
            &FileType::Other {
                nixpkgs_path: env!("NIXPKGS").to_string(),
                schema: HOME_MANAGER_SCHEMA.clone(),
            },
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_hover_lambda_let() {
        check_hover_with_filetype(
            r#"{ pkgs }: let four = 2+2; in { programs.zsh.enableAutosug$0gestions = false;  }"#,
            expect![[r#"
                no position

                ### option `programs.zsh.enableAutosuggestions`
                Alias of {option}`programs.zsh.autosuggestion.enable`.

                *Type:* boolean
            "#]],
            &FileType::Other {
                nixpkgs_path: env!("NIXPKGS").to_string(),
                schema: HOME_MANAGER_SCHEMA.clone(),
            },
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_hover_path() {
        check_hover(
            indoc! {"
            ## /test/hello/world/whatever.nix
            ./ot$0her.nix
            "},
            expect![[r#"
            /test/hello/world/other.nix:1:1

            ```
            /test/hello/world/other.nix
            ```"#]],
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_hover_default_nix_path() {
        check_hover_nixpkgs(
            "default.nix",
            indoc! {"
            import ./l$0ib
            "},
            expect![[r#"
                /nix/store/1gn7gki3wbgw5v4vcd349660gsd1qb43-source/lib/default.nix:1:1

                ```
                /nix/store/1gn7gki3wbgw5v4vcd349660gsd1qb43-source/lib
                ```"#]],
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_hover_nixpkgs_lib() {
        check_hover_nixpkgs(
            "lib/default.nix",
            indoc! {r#"
            let makeExtensible' = rattrs: let self = rattrs self // { extend = f: lib.makeExtensible (lib.extends f rattrs); }; in self;
            lib = makeExtensible' (self: let
                callLibs = file: import file { lib = self; };
            in {
                trivial = callLibs ./trivial.nix;
                inherit (self.trivial) i$0d;
            }
            "#},
            expect![[r#"
                /nix/store/1gn7gki3wbgw5v4vcd349660gsd1qb43-source/lib/default.nix:6:4

                ### function

                ```nix
                «lambda id @ /nix/store/1gn7gki3wbgw5v4vcd349660gsd1qb43-source/lib/trivial.nix:39:8»
                ```"#]],
        )
        .await;
    }
}
