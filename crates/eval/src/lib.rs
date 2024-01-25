use value::{EnvId, ValueId};

mod db;
mod value;

#[salsa::jar(db = Db)]
pub struct Jar(ValueId, EnvId);

pub trait Db: salsa::DbWithJar<Jar> {}
impl<DB> Db for DB where DB: ?Sized + salsa::DbWithJar<Jar> {}
