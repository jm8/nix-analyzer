use rnix::ast::{Expr, Lambda};
use rowan::ast::AstNode;

use crate::{
    file_types::{FileInfo, FileType, LockedFlake},
    nixpkgs_lambda_arg::get_nixpkgs_lambda_arg,
    safe_stringification::{safe_stringify_opt, safe_stringify_opt_param},
    syntax::{ancestor_exprs, parse},
};

pub fn get_lambda_arg(lambda: &Lambda, file_info: &FileInfo) -> String {
    if let FileInfo {
        file_type:
            FileType::Nixpkgs {
                nixpkgs_path,
                relative_path,
            },
        ..
    } = file_info
    {
        if let Some(lambda) = get_nixpkgs_lambda_arg(lambda, nixpkgs_path, relative_path) {
            return lambda;
        }
    }

    if is_root_lambda(lambda) {
        if let Some(root_lambda) = get_root_lambda(file_info) {
            return root_lambda;
        }
    }
    if safe_stringify_opt_param(lambda.param().as_ref(), file_info.base_path()) == "system" {
        return "builtins.currentSystem".to_string();
    }
    "{}".to_owned()
}

pub fn get_root_lambda(file_info: &FileInfo) -> Option<String> {
    match &file_info.file_type {
        FileType::Other { nixpkgs_path, .. } => Some(format!("(import {} {{}})", nixpkgs_path)),
        FileType::Custom { lambda_arg, .. } => Some(safe_stringify_opt(
            parse(lambda_arg).expr().as_ref(),
            file_info.base_path(),
        )),
        FileType::Flake {
            locked: LockedFlake::Pending | LockedFlake::None,
        } => Some("{}".to_string()),
        FileType::Flake {
            locked:
                LockedFlake::Locked {
                    lock_file: _,
                    inputs,
                },
        } => Some(inputs.clone()),
        FileType::Nixpkgs { .. } => None, // This is handled by get_nixpkgs_lambda_arg
    }
}

pub fn is_root_lambda(lambda: &Lambda) -> bool {
    ancestor_exprs(&Expr::cast(lambda.syntax().clone()).unwrap())
        .all(|e| !matches!(e, Expr::Lambda(_)))
}
