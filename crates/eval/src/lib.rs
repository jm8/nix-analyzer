use eval::{force_value, maybe_thunk};
use value::{lookup_bindings, lookup_env, BindingsId, EnvId, ExprId, ValueId};

mod db;
mod eval;
mod tests;
mod value;

#[salsa::jar(db = Db)]
pub struct Jar(
    ValueId,
    EnvId,
    ExprId,
    BindingsId,
    lookup_bindings,
    lookup_env,
    force_value,
    maybe_thunk,
    InputExpr,
);

pub trait Db: salsa::DbWithJar<Jar> {}
impl<DB> Db for DB where DB: ?Sized + salsa::DbWithJar<Jar> {}

#[salsa::input]
struct InputExpr {
    expr: ExprId,
}
