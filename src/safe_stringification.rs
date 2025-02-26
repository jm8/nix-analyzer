use std::path::{Path, PathBuf};

use itertools::Itertools;
use rnix::ast::{self, Attr, Attrpath, Expr, HasEntry};

use crate::walk_attrs::{walk_attrs, Subscript};

/// Convert an expression to a string while guaranteeing that the
/// resulting string is syntactically correct.
pub fn safe_stringify(expr: &Expr, base_path: &Path) -> String {
    match expr {
        Expr::Apply(apply) => format!(
            "({} {})",
            safe_stringify_opt(apply.lambda().as_ref(), base_path),
            safe_stringify_opt(apply.argument().as_ref(), base_path)
        ),
        Expr::Assert(assert) => format!(
            "(assert {}; {})",
            safe_stringify_opt(assert.condition().as_ref(), base_path),
            safe_stringify_opt(assert.body().as_ref(), base_path)
        ),
        Expr::Error(_) => "null".to_string(),
        Expr::IfElse(if_else) => format!(
            "(if {} then {} else {})",
            safe_stringify_opt(if_else.condition().as_ref(), base_path),
            safe_stringify_opt(if_else.body().as_ref(), base_path),
            safe_stringify_opt(if_else.else_body().as_ref(), base_path)
        ),
        Expr::Select(select) => format!(
            "({}.{}{})",
            safe_stringify_opt(select.expr().as_ref(), base_path),
            safe_stringify_opt_attrpath(select.attrpath().as_ref(), base_path),
            match select.default_expr() {
                Some(default_expr) => format!(" or {}", safe_stringify(&default_expr, base_path)),
                None => "".to_string(),
            }
        ),
        Expr::Str(str_node) => safe_stringify_str(str_node, base_path),
        Expr::Path(path) => {
            let s = path.to_string();
            if s.starts_with("<") && s.ends_with(">") {
                s
            } else {
                let path = PathBuf::from(s);
                if path.is_relative() {
                    base_path.join(path).to_string_lossy().to_string()
                } else {
                    path.to_string_lossy().to_string()
                }
            }
        }
        Expr::Literal(literal) => literal.to_string(),
        Expr::Lambda(lambda) => {
            let arg = safe_stringify_opt_param(lambda.param().as_ref(), base_path);
            format!(
                "({}: {})",
                arg,
                safe_stringify_opt(lambda.body().as_ref(), base_path)
            )
        }
        Expr::LegacyLet(let_legacy) => {
            format!("let {}", safe_stringify_bindings(let_legacy, base_path))
        }
        Expr::LetIn(let_in) => format!(
            "(let {} in {})",
            safe_stringify_bindings(let_in, base_path),
            safe_stringify_opt(let_in.body().as_ref(), base_path)
        ),
        Expr::List(list) => format!(
            "[{}]",
            list.items()
                .map(|e| safe_stringify(&e, base_path))
                .join(" ")
        ),
        Expr::BinOp(bin_op) => {
            let Some(operator) = bin_op.operator() else {
                return "null".to_string();
            };
            let left_expr = safe_stringify_opt(bin_op.lhs().as_ref(), base_path);
            let right_expr = safe_stringify_opt(bin_op.rhs().as_ref(), base_path);

            match operator {
                ast::BinOpKind::Concat => format!("({} ++ {})", left_expr, right_expr),
                ast::BinOpKind::Add => format!("({} + {})", left_expr, right_expr),
                ast::BinOpKind::Sub => format!("({} - {})", left_expr, right_expr),
                ast::BinOpKind::Mul => format!("({} * {})", left_expr, right_expr),
                ast::BinOpKind::Div => format!("({} / {})", left_expr, right_expr),
                ast::BinOpKind::And => format!("({} && {})", left_expr, right_expr),
                ast::BinOpKind::Equal => format!("({} == {})", left_expr, right_expr),
                ast::BinOpKind::Implication => format!("({} => {})", left_expr, right_expr),
                ast::BinOpKind::Less => format!("({} < {})", left_expr, right_expr),
                ast::BinOpKind::LessOrEq => format!("({} <= {})", left_expr, right_expr),
                ast::BinOpKind::More => format!("({} > {})", left_expr, right_expr),
                ast::BinOpKind::MoreOrEq => format!("({} >= {})", left_expr, right_expr),
                ast::BinOpKind::NotEqual => format!("({} != {})", left_expr, right_expr),
                ast::BinOpKind::Or => format!("({} || {})", left_expr, right_expr),
                ast::BinOpKind::Update => format!("({} // {})", left_expr, right_expr),
                ast::BinOpKind::PipeRight => format!("({} |> {})", left_expr, right_expr),
                ast::BinOpKind::PipeLeft => format!("({} <| {})", left_expr, right_expr),
            }
        }
        Expr::Paren(paren) => format!("({})", safe_stringify_opt(paren.expr().as_ref(), base_path)),
        Expr::Root(root) => safe_stringify_opt(root.expr().as_ref(), base_path),
        Expr::AttrSet(attrset) => {
            format!(
                "{}{{ {} }}",
                if attrset.rec_token().is_some() {
                    "rec "
                } else {
                    ""
                },
                safe_stringify_bindings(attrset, base_path)
            )
        }
        Expr::UnaryOp(unary_op) => {
            let Some(operator) = unary_op.operator() else {
                return safe_stringify_opt(unary_op.expr().as_ref(), base_path);
            };
            match operator {
                ast::UnaryOpKind::Invert => {
                    format!(
                        "(!{})",
                        safe_stringify_opt(unary_op.expr().as_ref(), base_path)
                    )
                }
                ast::UnaryOpKind::Negate => {
                    format!(
                        "(-{})",
                        safe_stringify_opt(unary_op.expr().as_ref(), base_path)
                    )
                }
            }
        }
        Expr::Ident(ident) => ident.to_string(),
        Expr::With(with) => format!(
            "(with {}; {})",
            safe_stringify_opt(with.namespace().as_ref(), base_path),
            safe_stringify_opt(with.body().as_ref(), base_path)
        ),
        Expr::HasAttr(has_attr) => format!(
            "({} ? {})",
            safe_stringify_opt(has_attr.expr().as_ref(), base_path),
            safe_stringify_opt_attrpath(has_attr.attrpath().as_ref(), base_path)
        ),
    }
}

pub fn safe_stringify_opt_param(param: Option<&ast::Param>, base_path: &Path) -> String {
    match param {
        Some(ast::Param::Pattern(pattern)) => safe_stringify_pattern(pattern, base_path),
        Some(ast::Param::IdentParam(ident_param)) => ident_param.to_string(),
        None => "{...}".to_string(),
    }
}

pub fn safe_stringify_pattern(pattern: &rnix::ast::Pattern, base_path: &Path) -> String {
    let entries = pattern
        .pat_entries()
        .flat_map(|pat_entry| {
            pat_entry.ident().map(|ident| {
                format!(
                    "{} ? {}",
                    ident,
                    safe_stringify_opt(pat_entry.default().as_ref(), base_path)
                )
            })
        })
        .join(", ");

    match pattern.pat_bind().and_then(|pat_bind| pat_bind.ident()) {
        Some(ident) => format!("{{{}, ...}} @ {}", entries, ident),
        None => format!("{{{}, ...}}", entries),
    }
}

pub fn safe_stringify_str(str: &rnix::ast::Str, _base_path: &Path) -> String {
    // TODO: fix
    str.to_string()
}

pub fn safe_stringify_attr(attr: &Attr, base_path: &Path) -> String {
    match attr {
        Attr::Ident(ident) => {
            let s = ident.to_string();
            if s.is_empty() {
                "null".to_string()
            } else {
                s
            }
        }
        Attr::Dynamic(dynamic) => format!(
            "${{{}}}",
            safe_stringify_opt(dynamic.expr().as_ref(), base_path),
        ),
        Attr::Str(str) => safe_stringify_str(str, base_path),
    }
}

pub fn safe_stringify_bindings(bindings: &impl HasEntry, base_path: &Path) -> String {
    bindings
        .entries()
        .map(|entry| match entry {
            ast::Entry::Inherit(inherit) => format!(
                "inherit {} {};",
                match inherit.from() {
                    Some(inherit_from) => format!(
                        "({})",
                        safe_stringify_opt(inherit_from.expr().as_ref(), base_path),
                    ),
                    None => String::new(),
                },
                inherit
                    .attrs()
                    .map(|attr| safe_stringify_attr(&attr, base_path))
                    .join(" ")
            ),
            ast::Entry::AttrpathValue(attrpath_value) => format!(
                "{} = {};",
                safe_stringify_opt_attrpath(attrpath_value.attrpath().as_ref(), base_path),
                safe_stringify_opt(attrpath_value.value().as_ref(), base_path),
            ),
        })
        .join(" ")
}

pub fn safe_stringify_attrpath(expr: &Attrpath, base_path: &Path) -> String {
    let s = expr
        .attrs()
        .map(|attr| safe_stringify_attr(&attr, base_path))
        .join(".");
    if s.is_empty() {
        "null".to_string()
    } else {
        s
    }
}

pub fn safe_stringify_opt_attrpath(attrpath: Option<&Attrpath>, base_path: &Path) -> String {
    match attrpath {
        Some(attrpath) => safe_stringify_attrpath(attrpath, base_path),
        None => "null".to_string(),
    }
}

pub fn safe_stringify_opt(expr: Option<&Expr>, base_path: &Path) -> String {
    match expr {
        Some(expr) => safe_stringify(expr, base_path),
        None => "null".to_string(),
    }
}

pub fn safe_stringify_flake(expr: Option<&Expr>, base_path: &Path) -> String {
    match expr {
        Some(Expr::AttrSet(attrs)) => {
            format!(
                "{{ {} }}",
                walk_attrs(attrs)
                    .iter()
                    .filter(|item| item.position.first()
                        == Some(&Subscript::Attr("inputs".to_string())))
                    .filter(|item| {
                        item.position
                            .iter()
                            .all(|x| matches!(x, Subscript::Attr(_)))
                    })
                    .map(|item| format!(
                        "{} = {};",
                        item.position
                            .iter()
                            .map(|x| match x {
                                Subscript::Attr(attr) => attr,
                                _ => unreachable!(),
                            })
                            .join("."),
                        safe_stringify(&item.expr, base_path),
                    ))
                    .join(" ")
            )
        }

        _ => "{}".to_string(),
    }
}

#[cfg(test)]
mod test {
    use expect_test::{expect, Expect};

    use super::safe_stringify_opt;
    use crate::syntax::parse;

    fn check(source: &str, expected: Expect) {
        expected.assert_eq(&safe_stringify_opt(
            parse(source).expr().as_ref(),
            "/base_path".as_ref(),
        ));
    }

    #[test]
    fn assert() {
        check(r#"assert x; y"#, expect!["(assert x; y)"]);
    }

    #[test]
    fn attrs_dup1() {
        check(
            r#"{ x = 123; y = 456; x = 789; }"#,
            expect!["{ x = 123; y = 456; x = 789; }"],
        );
    }

    #[test]
    fn attrs_dup2() {
        check(
            r#"{ services.ssh.port = 22; services.ssh.port = 23; }"#,
            expect!["{ services.ssh.port = 22; services.ssh.port = 23; }"],
        );
    }

    #[test]
    fn attrs_dynamic() {
        check(r#"{ ${a} = b; }"#, expect!["{ ${a} = b; }"]);
    }

    #[test]
    fn attrs_empty() {
        check(r#"{ }"#, expect!["{  }"]);
    }

    #[test]
    fn attrs_mixed_nested() {
        check(
            r#"{
              services.ssh.enable = true;
              services.ssh = { port = 123; };
              services = {
                  httpd.enable = true;
              };
            }"#,
            expect!["{ services.ssh.enable = true; services.ssh = { port = 123; }; services = { httpd.enable = true; }; }"],
        );
    }

    #[test]
    fn attrs_rec() {
        check(r#"rec {  }"#, expect!["rec {  }"]);
    }

    #[test]
    fn attrs_simple() {
        check(r#"{ x = y; y = z; }"#, expect!["{ x = y; y = z; }"]);
    }

    #[test]
    fn attrs_string() {
        check(r#"{ "a" = b; }"#, expect![[r#"{ "a" = b; }"#]]);
    }

    #[test]
    fn attrs_typing1() {
        // Ideally this would be an attribute set not a lambda
        check(r#"{ x }"#, expect!["({x ? null, ...}: null)"]);
    }

    #[test]
    fn attrs_typing2() {
        check(r#"{ x. }"#, expect!["{ x = null; }"]);
    }

    #[test]
    fn attrs_typing3() {
        check(r#"{ x.y }"#, expect!["{ x.y = null; }"]);
    }

    #[test]
    fn attrs_typing4() {
        check(r#"{ x.y = }"#, expect!["{ x.y = null; }"]);
    }

    #[test]
    fn attrs_typing5() {
        check(r#"{ x.y = abc }"#, expect!["{ x.y = abc; }"]);
    }

    #[test]
    fn attrs_typing_before() {
        check(
            r#"{ services.resolved. networking.useDHCP = false; }"#,
            expect!["{ services.resolved.networking.useDHCP = false; }"],
        );
    }

    #[test]
    fn call_basic() {
        check(r#"function a b c d"#, expect!["((((function a) b) c) d)"]);
    }

    #[test]
    fn comma_attrs() {
        check(r#"{a = 1, b = 2}"#, expect!["{ a = 1; }"]);
    }

    #[test]
    fn id() {
        check(r#"hello"#, expect!["hello"]);
    }

    #[test]
    fn if_not_allowed() {
        check(r#"a if b then c else d"#, expect!["a"]);
    }

    #[test]
    fn ind_string() {
        check(
            r#"
            ''
              abc
              ${hello world}
            ''"#,
            expect![[r#"
                ''
                              abc
                              ${hello world}
                            ''"#]],
        );
    }

    #[test]
    fn inherit() {
        check(
            r#"{ inherit a; inherit (x) b; }"#,
            expect!["{ inherit  a; inherit (x) b; }"],
        );
    }

    #[test]
    fn inherit_or() {
        check(r#"{ inherit or; }"#, expect!["{ inherit  or; }"]);
    }

    #[test]
    fn lambda_formals() {
        check(r#"{a, b}: a"#, expect!["({a ? null, b ? null, ...}: a)"]);
    }

    #[test]
    fn lambda_formals_arg() {
        check(
            r#"{a, b, ...} @ g: a"#,
            expect!["({a ? null, b ? null, ...} @ g: a)"],
        );
    }

    #[test]
    fn lambda_formals_arg_left() {
        check(
            r#"g@{a, b, ...}: a"#,
            expect!["({a ? null, b ? null, ...} @ g: a)"],
        );
    }

    #[test]
    fn lambda_formals_dup() {
        check(
            r#"{a, b, a}: a"#,
            expect!["({a ? null, b ? null, a ? null, ...}: a)"],
        );
    }

    #[test]
    fn lambda_formals_empty() {
        check(r#"{}: a"#, expect!["({, ...}: a)"]);
    }

    #[test]
    fn lambda_formals_one() {
        check(r#"{a}: a"#, expect!["({a ? null, ...}: a)"]);
    }

    #[test]
    fn lambda_regular() {
        check(r#"x: y: x"#, expect!["(x: (y: x))"]);
    }

    #[test]
    fn let_dynamic() {
        check(r#"let ${a} = b; in c"#, expect!["(let ${a} = b; in c)"]);
    }

    #[test]
    fn let_typing_missing_in() {
        // Not ideal
        check(r#"let a = 4; b"#, expect!["(let a = 4; b = null; in null)"]);
    }

    #[test]
    fn let_typing_missing_semicolon() {
        // Not ideal
        check(r#"let a = 4 in b"#, expect!["(let a = 4; in null)"]);
    }

    #[test]
    fn op_non_assoc() {
        check(r#"1 < 2 < 3"#, expect!["(1 < 2)"]);
    }

    #[test]
    fn op_prec() {
        check(
            r#"1.5 -> 2 || -3 && 4 == 5 || 6 < 7 || 8 // !9 + 10 * 11 ++ 12 13 ? a // 1"#,
            expect!["(1.5 => (((2 || ((-3) && (4 == 5))) || (6 < 7)) || (8 // ((!(9 + (10 * (11 ++ ((12 13) ? a))))) // 1))))"],
        );
    }

    #[test]
    fn path_absolute() {
        check(r#"/etc/passwd"#, expect!["/etc/passwd"]);
    }

    #[test]
    fn path_interpolated() {
        check(
            r#"/home/${username}/whatever"#,
            expect!["/home/${username}/whatever"],
        );
    }

    #[test]
    fn path_relative() {
        check(
            r#"./parse_assert.nix"#,
            expect!["/base_path/./parse_assert.nix"],
        );
    }

    #[test]
    fn path_trailing_slash() {
        check(r#"/home/"#, expect!["null"]);
    }

    #[test]
    fn select_dynamic() {
        check(r#"hello.${12}"#, expect!["(hello.${12})"]);
    }

    #[test]
    fn select_empty() {
        check(r#"hello.whatever."#, expect!["(hello.whatever)"]);
    }

    #[test]
    fn select_or() {
        // This is a bug in rnix??
        check(r#"or.foo or foo"#, expect!["(null.foo or foo)"]);
    }

    #[test]
    fn select_parens() {
        check(
            r#"(import /nix/store/xif4dbqvi7bmcwfxiqqhq0nr7ax07liw-source)."#,
            expect!["(((import /nix/store/xif4dbqvi7bmcwfxiqqhq0nr7ax07liw-source)).null)"],
        );
    }

    #[test]
    fn select_string() {
        check(
            r#"hello."abc"."x${yz}""#,
            expect![[r#"(hello."abc"."x${yz}")"#]],
        );
    }

    #[test]
    fn string() {
        check(r#""hello ${abc} ${}""#, expect![[r#""hello ${abc} ${}""#]]);
    }

    #[test]
    fn subtraction() {
        check(r#"- n - 1"#, expect!["((-n) - 1)"]);
    }

    #[test]
    fn with_in_list() {
        check(r#"[ a b with c; d e]"#, expect!["[a b null c null d e]"]);
    }

    #[test]
    fn with_missing() {
        check(r#"with pkgs;"#, expect!["(with pkgs; null)"]);
    }

    #[test]
    fn lambda_basic() {
        check(r#"x: x"#, expect!["(x: x)"]);
    }

    #[test]
    fn lambda_args() {
        check(r#"{a, b}: a"#, expect!["({a ? null, b ? null, ...}: a)"]);
    }

    #[test]
    fn lambda_default_args() {
        check(
            r#"{a ? 123, b}: a"#,
            expect!["({a ? 123, b ? null, ...}: a)"],
        );
    }

    #[test]
    fn lambda_bind() {
        check(
            r#"{a ? 123, b} @ args: a"#,
            expect!["({a ? 123, b ? null, ...} @ args: a)"],
        );
    }

    #[test]
    fn lambda_bind_2() {
        check(
            r#"args @ {a ? 123, b}: a"#,
            expect!["({a ? 123, b ? null, ...} @ args: a)"],
        );
    }

    #[test]
    fn empty_select() {
        check(r#"x."#, expect!["(x.null)"]);
    }

    #[test]
    fn select_containing_empty() {
        check(r#"x.a..b"#, expect!["(x.a.b)"]);
    }

    #[test]
    fn select_empty_end() {
        check(r#"x.a."#, expect!["(x.a)"]);
    }
}
