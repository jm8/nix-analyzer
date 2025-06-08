use crate::schema::{Schema, HOME_MANAGER_SCHEMA};
use std::{
    ffi::OsStr,
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
    Other {
        nixpkgs_path: String,
        schema: Arc<Schema>,
    },
    Nixpkgs {
        nixpkgs_path: PathBuf,
        relative_path: PathBuf,
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
    if path.file_name() == Some(OsStr::new("flake.nix")) {
        return FileInfo {
            file_type: FileType::Flake {
                locked: LockedFlake::None,
            },
            path: path.to_path_buf(),
        };
    }

    if let Some(nixpkgs_path) = get_nixpkgs_path(path) {
        return FileInfo {
            file_type: FileType::Nixpkgs {
                relative_path: path.strip_prefix(&nixpkgs_path).unwrap().to_path_buf(),
                nixpkgs_path,
            },
            path: path.to_path_buf(),
        };
    }

    FileInfo {
        file_type: FileType::Other {
            nixpkgs_path: env!("NIXPKGS").to_owned(),
            schema: HOME_MANAGER_SCHEMA.clone(),
        },
        path: path.to_path_buf(),
    }
}

fn get_nixpkgs_path(path: &Path) -> Option<PathBuf> {
    for p in path.ancestors() {
        if p.join("pkgs/top-level/all-packages.nix").exists() {
            return Some(p.to_owned());
        }
    }
    return None;
}
