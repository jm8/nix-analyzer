use crate::{
    value::{BindingsId, EnvId, ExprId, ValueId, ValueKind},
    FileId,
};
use derive_new::new;
use rnix::ast::Expr;

#[derive(new, PartialEq, Eq, Debug, Hash, Clone)]
pub struct Diagnostic {
    pub expr: ExprId,
    pub message: String,
}

#[salsa::accumulator]
pub struct Diagnostics(Diagnostic);

#[salsa::tracked]
pub fn parse(db: &dyn crate::Db, file: FileId) -> ExprId {
    let contents = file.contents(db);
    ExprId::new(db, rnix::Root::parse(&contents).tree().expr())
}

#[salsa::tracked]
pub fn maybe_thunk(db: &dyn crate::Db, expr: ExprId, env: EnvId) -> ValueId {
    todo!()
}

#[salsa::tracked]
pub fn base_env(db: &dyn crate::Db) -> EnvId {
    EnvId::new(db, BindingsId::new(db, Default::default()), None)
}

#[salsa::tracked]
pub fn eval(db: &dyn crate::Db, expr_id: ExprId, env: EnvId) -> ValueId {
    let expr = expr_id.expr(db);

    let Some(expr) = expr else {
        return ValueId::new(db, ValueKind::Error);
    };

    match expr {
        Expr::Apply(_) => todo!(),
        Expr::Assert(_) => todo!(),
        Expr::Error(_) => todo!(),
        Expr::IfElse(_) => todo!(),
        Expr::Select(_) => todo!(),
        Expr::Str(_) => todo!(),
        Expr::Path(_) => todo!(),
        Expr::Literal(x) => match x.kind() {
            rnix::ast::LiteralKind::Float(_) => todo!(),
            rnix::ast::LiteralKind::Integer(a) => match a.value() {
                Ok(n) => ValueId::new(db, ValueKind::Int(n)),
                Err(err) => {
                    Diagnostics::push(db, Diagnostic::new(expr_id, err.to_string()));
                    ValueId::new(db, ValueKind::Error)
                }
            },
            rnix::ast::LiteralKind::Uri(_) => todo!(),
        },
        Expr::Lambda(_) => todo!(),
        Expr::LegacyLet(_) => todo!(),
        Expr::LetIn(_) => todo!(),
        Expr::List(_) => todo!(),
        Expr::BinOp(_) => todo!(),
        Expr::Paren(paren) => eval(db, ExprId::new(db, paren.expr()), env),
        Expr::Root(_) => todo!(),
        Expr::AttrSet(_) => todo!(),
        Expr::UnaryOp(_) => todo!(),
        Expr::Ident(_) => todo!(),
        Expr::With(_) => todo!(),
        Expr::HasAttr(_) => todo!(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use expect_test::expect;
    use salsa::DebugWithDb;

    fn test_eval(
        expr: &str,
        expected_value: expect_test::Expect,
        expected_diagnostics: expect_test::Expect,
    ) {
        let mut db = crate::db::Database::default();
        let file = FileId::new(&mut db, "/default.nix".into(), expr.into());
        let expr = parse(&db, file);
        let env = base_env(&db);

        let value = eval(&db, expr, env);
        let diagnostics = eval::accumulated::<Diagnostics>(&db, expr, env);

        expected_value.assert_debug_eq(&value.debug_with(&db, false));
        expected_diagnostics.assert_debug_eq(&diagnostics);
    }

    #[test]
    fn test_eval_int() {
        test_eval(
            "2",
            expect![[r#"
                ValueId {
                    [salsa id]: 0,
                    kind: Int(
                        2,
                    ),
                }
            "#]],
            expect![[r#"
                []
            "#]],
        );
    }

    #[test]
    fn test_eval_empty() {
        test_eval(
            "",
            expect![[r#"
                ValueId {
                    [salsa id]: 0,
                    kind: Error,
                }
            "#]],
            expect![[r#"
                []
            "#]],
        );
    }

    #[test]
    fn test_eval_int_too_big() {
        test_eval(
            "9999999999999999999999",
            expect![[r#"
                ValueId {
                    [salsa id]: 0,
                    kind: Error,
                }
            "#]],
            expect![[r#"
                [
                    Diagnostic {
                        expr: ExprId(
                            Id {
                                value: 1,
                            },
                        ),
                        message: "number too large to fit in target type",
                    },
                ]
            "#]],
        );
    }
}
