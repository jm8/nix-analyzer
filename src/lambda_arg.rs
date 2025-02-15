use rnix::ast::{Expr, Lambda};

use crate::FileType;

pub fn get_lambda_arg(lambda: &Lambda, file_type: &FileType) -> String {
    "{ test = 1; }".to_owned()
}
