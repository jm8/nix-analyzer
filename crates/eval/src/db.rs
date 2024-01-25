use salsa::DebugWithDb;

#[derive(Default)]
#[salsa::db(crate::Jar)]
pub(crate) struct Database {
    storage: salsa::Storage<Self>,
}

impl salsa::Database for Database {
    fn salsa_event(&self, event: salsa::Event) {
        if let salsa::EventKind::WillExecute { .. } = event.kind {
            eprintln!("Event: {:?}", event.debug(self));
        }
    }
}

impl salsa::ParallelDatabase for Database {
    fn snapshot(&self) -> salsa::Snapshot<Self> {
        salsa::Snapshot::new(Database {
            storage: self.storage.snapshot(),
        })
    }
}
