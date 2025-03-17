use crate::schema::{Schema, HOME_MANAGER_SCHEMA};
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
pub enum LockedFlake {
    None,
    Pending,
    Locked { lock_file: String, inputs: String },
}

#[derive(Debug, Clone)]
pub enum FileType {
    Package {
        nixpkgs_path: String,
        schema: Arc<Schema>,
    },
    Flake {
        locked: LockedFlake,
    },
    Custom {
        lambda_arg: String,
        schema: String,
    },
}

pub fn init_file_info(path: &Path, _source: &str) -> FileInfo {
    let default = FileType::Package {
        nixpkgs_path: env!("NIXPKGS").to_owned(),
        schema: HOME_MANAGER_SCHEMA.clone(),
    };
    FileInfo {
        file_type: if path.ends_with("flake.nix") {
            FileType::Flake {
                locked: LockedFlake::None,
            }
        } else {
            default
        },
        path: path.to_owned(),
    }
}
