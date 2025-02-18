use std::sync::Arc;

use crate::{
    evaluator::{Evaluator, LockFlakeRequest},
    file_types::FileType,
    safe_stringification::safe_stringify_opt,
    syntax::parse,
};
use anyhow::Result;
use tokio::sync::Mutex;

pub async fn get_flake_filetype(
    evaluator: Arc<Mutex<Evaluator>>,
    source: &str,
    _old_flake_lock: Option<&str>,
) -> Result<FileType> {
    let lock_file = evaluator
        .lock()
        .await
        .lock_flake(&LockFlakeRequest {
            expression: safe_stringify_opt(parse(source).expr().as_ref(), "/".as_ref()),
        })
        .await?;

    Ok(FileType::Flake { lock_file })
}

#[cfg(test)]
mod test {
    use expect_test::expect;

    use crate::evaluator::{self, LockFlakeRequest};

    #[test_log::test(tokio::test)]
    async fn test_lock_flake() {
        let mut evaluator = evaluator::Evaluator::new();
        let expr = r#"{ inputs.nixpkgs.url = "github:nixos/nixpkgs/67b8f2ca98f3bbc6f3b5f25cc28290111c921007"; }"#;
        let lock_file = evaluator
            .lock_flake(&LockFlakeRequest {
                expression: expr.to_owned(),
            })
            .await
            .unwrap();
        expect![[r#"
            {
              "nodes": {
                "nixpkgs": {
                  "locked": {
                    "lastModified": 1739576677,
                    "narHash": "sha256-8hW4ERFocCbxKptvySFQ2ydeQ4+kVxro4zjtpw21NvM=",
                    "owner": "nixos",
                    "repo": "nixpkgs",
                    "rev": "67b8f2ca98f3bbc6f3b5f25cc28290111c921007",
                    "type": "github"
                  },
                  "original": {
                    "owner": "nixos",
                    "repo": "nixpkgs",
                    "rev": "67b8f2ca98f3bbc6f3b5f25cc28290111c921007",
                    "type": "github"
                  }
                },
                "root": {
                  "inputs": {
                    "nixpkgs": "nixpkgs"
                  }
                }
              },
              "root": "root",
              "version": 7
            }"#]]
        .assert_eq(&lock_file);
    }
}
