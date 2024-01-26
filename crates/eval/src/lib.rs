use std::path::PathBuf;

use evaluation::{base_env, eval, maybe_thunk, parse, Diagnostics};
use value::{lookup_bindings, lookup_env, BindingsId, EnvId, ExprId, ValueId};

mod db;
mod evaluation;
mod value;

#[salsa::jar(db = Db)]
pub struct Jar(
    base_env,
    BindingsId,
    Diagnostics,
    EnvId,
    eval,
    ExprId,
    FileId,
    lookup_bindings,
    lookup_env,
    maybe_thunk,
    parse,
    ValueId,
);

pub trait Db: salsa::DbWithJar<Jar> {}
impl<DB> Db for DB where DB: ?Sized + salsa::DbWithJar<Jar> {}

#[salsa::input]
pub struct FileId {
    path: PathBuf,
    #[return_ref]
    contents: String,
}
