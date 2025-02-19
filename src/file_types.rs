use tokio::sync::Mutex;

use crate::{evaluator::Evaluator, flakes::get_flake_filetype, schema::Schema};
use std::{
    path::{Path, PathBuf},
    sync::Arc,
};

#[derive(Debug, Clone)]
pub struct FileInfo {
    pub file_type: FileType,
    pub path: PathBuf,
}

impl FileInfo {
    pub fn base_path(&self) -> &Path {
        self.path.parent().unwrap_or("/".as_ref())
    }
}

#[derive(Debug, Clone)]
pub enum FileType {
    Package {
        nixpkgs_path: String,
        schema: Arc<Schema>,
    },
    Flake {
        lock_file: String,
    },
    Custom {
        lambda_arg: String,
        schema: String,
    },
}

pub async fn get_file_info(
    evaluator: Arc<Mutex<Evaluator>>,
    path: &Path,
    source: &str,
    temp_nixos_module_schema: Arc<Schema>,
) -> FileInfo {
    let default = FileType::Package {
        nixpkgs_path: env!("nixpkgs").to_owned(),
        schema: temp_nixos_module_schema.clone(),
    };
    FileInfo {
        file_type: if path.ends_with("flake.nix") {
            get_flake_filetype(evaluator, source, None)
                .await
                .unwrap_or(default)
        } else {
            default
        },
        path: path.to_owned(),
    }
}
