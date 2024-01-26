use rnix::ast::Expr;
use sorted_vec::SortedVec;
type Symbol = ustr::Ustr;

#[salsa::interned]
pub struct ValueId {
    pub kind: ValueKind,
}

#[salsa::interned]
pub struct ExprId {
    pub expr: Option<Expr>,
}

#[derive(PartialEq, Eq, Hash, Clone, Debug)]
pub enum ValueKind {
    Int(i64),
    Thunk(ExprId, EnvId),
    Error,
}

#[salsa::interned]
pub struct EnvId {
    bindings: BindingsId,
    up: Option<EnvId>,
}

#[salsa::interned]
pub struct BindingsId {
    #[return_ref]
    bindings: SortedVec<(Symbol, ValueId)>,
}

#[salsa::tracked]
pub fn lookup_env(db: &dyn crate::Db, env_id: EnvId, key: Symbol) -> Option<ValueId> {
    let mut curr = Some(env_id);
    while let Some(env) = curr {
        let bindings = env.bindings(db);
        let res = lookup_bindings(db, bindings, key);
        if res.is_some() {
            return res;
        }
        curr = env.up(db);
    }
    None
}

#[salsa::tracked]
pub fn lookup_bindings(
    db: &dyn crate::Db,
    bindings_id: BindingsId,
    key: Symbol,
) -> Option<ValueId> {
    let bindings = bindings_id.bindings(db);
    bindings.iter().find(|(k, _v)| *k == key).map(|(_k, v)| *v)
}

#[test]
fn test_bindings() {
    let db = crate::db::Database::default();
    let a = ValueId::new(&db, ValueKind::Int(10));
    let bindings = BindingsId::new(&db, vec![("Hello".into(), a)].into());

    let v = lookup_bindings(&db, bindings, "Hello".into());
    assert_eq!(v, Some(a));
}

#[test]
fn test_env_bindings() {
    let db = crate::db::Database::default();
    let one = ValueId::new(&db, ValueKind::Int(1));
    let two = ValueId::new(&db, ValueKind::Int(2));
    let a = BindingsId::new(&db, vec![("x".into(), one)].into());
    let b = BindingsId::new(&db, vec![("x".into(), two), ("y".into(), two)].into());
    let envb = EnvId::new(&db, b, None);
    let enva = EnvId::new(&db, a, Some(envb));

    assert_eq!(lookup_env(&db, enva, "x".into()), Some(one));
    assert_eq!(lookup_env(&db, enva, "y".into()), Some(two));
    assert_eq!(lookup_env(&db, enva, "z".into()), None);
}
