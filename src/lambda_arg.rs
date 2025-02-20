use rnix::ast::{Expr, Lambda};
use rowan::ast::AstNode;

use crate::{
    file_types::{FileInfo, FileType, LockedFlake},
    safe_stringification::safe_stringify_opt,
    syntax::{ancestor_exprs, escape_string, parse},
};

pub fn get_lambda_arg(lambda: &Lambda, file_info: &FileInfo) -> String {
    if is_root_lambda(lambda) {
        if let Some(root_lambda) = get_root_lambda(file_info) {
            return root_lambda;
        }
    }
    "{}".to_owned()
}

pub fn get_root_lambda(file_info: &FileInfo) -> Option<String> {
    match &file_info.file_type {
        FileType::Package { nixpkgs_path, .. } => Some(format!("(import {} {{}})", nixpkgs_path)),
        FileType::Custom { lambda_arg, .. } => Some(safe_stringify_opt(
            parse(lambda_arg).expr().as_ref(),
            file_info.base_path(),
        )),
        FileType::Flake {
            locked: LockedFlake::Pending | LockedFlake::None,
        } => Some("{}".to_string()),
        FileType::Flake {
            locked: LockedFlake::Locked { lock_file },
        } => Some(format!(
            "(import /var/home/josh/src/nix-analyzer-new/src/get-flake-inputs.nix {} {{}})",
            escape_string(lock_file),
        )),
    }
}

pub fn is_root_lambda(lambda: &Lambda) -> bool {
    ancestor_exprs(&Expr::cast(lambda.syntax().clone()).unwrap())
        .all(|e| !matches!(e, Expr::Lambda(_)))
}
