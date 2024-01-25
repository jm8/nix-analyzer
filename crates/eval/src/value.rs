use std::collections::HashMap;
use syntax::{Expr, Symbol};

#[salsa::interned]
pub struct ValueId {
    kind: ValueKind,
}

#[salsa::interned]
pub struct ExprId {
    expr: Expr,
}

#[derive(PartialEq, Eq, Hash, Clone, Debug)]
pub enum ValueKind {
    Int(i64),
    Thunk(ExprId, EnvId),
}

#[salsa::tracked]
pub struct EnvId {
    bindings: HashMap<Symbol, ValueId>,
    up: EnvId,
}
