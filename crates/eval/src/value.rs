use std::collections::HashMap;
use syntax::Symbol;

#[salsa::interned]
pub struct ValueId {
    kind: ValueKind,
}

#[derive(PartialEq, Eq, Hash, Clone, Debug)]
enum ValueKind {
    Int(i64),
    Thunk(ValueId, EnvId),
}

#[salsa::tracked]
pub struct EnvId {
    bindings: HashMap<Symbol, ValueId>,
    up: EnvId,
}
