use value::{EnvId, ExprId, ValueId};

mod db;
mod value;

#[salsa::jar(db = Db)]
pub struct Jar(ValueId, EnvId, ExprId);

pub trait Db: salsa::DbWithJar<Jar> {}
impl<DB> Db for DB where DB: ?Sized + salsa::DbWithJar<Jar> {}
