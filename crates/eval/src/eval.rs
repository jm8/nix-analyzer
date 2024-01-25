use crate::value::{EnvId, ValueId};

#[salsa::tracked]
pub fn maybe_thunk(db: &dyn crate::Db, value: ValueId, env: EnvId) -> ValueId {
    todo!()
}

#[salsa::tracked]
pub fn force_value(db: &dyn crate::Db, value: ValueId, env: EnvId) -> ValueId {
    todo!()
}
