use std::sync::Arc;

use crate::schema::Schema;

#[derive(Debug)]
pub enum FileType {
    Package {
        nixpkgs_path: String,
        schema: Arc<Schema>,
    },
    Custom {
        lambda_arg: String,
        schema: String,
    },
}
