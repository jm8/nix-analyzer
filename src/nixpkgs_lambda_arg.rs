use rnix::ast::Lambda;
use std::path::{Path, PathBuf};

use crate::{lambda_arg::is_root_lambda, safe_stringification::safe_stringify_opt_param};

pub fn get_nixpkgs_lambda_arg(
    lambda: &Lambda,
    nixpkgs_path: &Path,
    relative_path: &Path,
) -> Option<String> {
    let param = safe_stringify_opt_param(lambda.param().as_ref(), nixpkgs_path);
    let nixpkgs = format!("(import {} {{}})", nixpkgs_path.display());

    if relative_path == &PathBuf::from("lib/default.nix") {
        if param == "self" {
            return Some(format!("{}.lib", nixpkgs));
        }
    }

    if is_root_lambda(lambda) {
        return Some(nixpkgs);
    }

    None
}
