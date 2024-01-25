use crate::value::{EnvId, ExprId, ValueId};

#[salsa::tracked]
pub fn maybe_thunk(db: &dyn crate::Db, expr: ExprId, env: EnvId) -> ValueId {
    todo!()
}

#[salsa::tracked]
pub fn eval(db: &dyn crate::Db, expr: ExprId, env: EnvId) -> ValueId {
    todo!()
}
