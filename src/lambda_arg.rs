use rnix::ast::{Expr, Lambda};
use rowan::ast::AstNode;

use crate::{
    safe_stringification::{safe_stringify, safe_stringify_opt},
    syntax::{ancestor_exprs, parse},
    FileType,
};

pub fn get_lambda_arg(lambda: &Lambda, file_type: &FileType) -> String {
    if is_root_lambda(lambda) {
        if let Some(root_lambda) = get_root_lambda(file_type) {
            return root_lambda;
        }
    }
    "null".to_owned()
}

pub fn get_root_lambda(file_type: &FileType) -> Option<String> {
    match file_type {
        FileType::Package { nixpkgs_path } => Some(format!("(import {} {{}})", nixpkgs_path)),
        FileType::Custom { lambda_arg } => {
            Some(safe_stringify_opt(parse(&lambda_arg).expr().as_ref()))
        }
    }
}

pub fn is_root_lambda(lambda: &Lambda) -> bool {
    ancestor_exprs(&Expr::cast(lambda.syntax().clone()).unwrap())
        .all(|e| !matches!(e, Expr::Lambda(_)))
}
