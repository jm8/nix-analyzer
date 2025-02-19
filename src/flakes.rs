use std::sync::Arc;

use crate::{
    evaluator::{Evaluator, GetAttributesRequest, LockFlakeRequest},
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
    let expression = safe_stringify_flake(source);
    // let lock_file = evaluator
    //     .lock()
    //     .await
    //     .lock_flake(&LockFlakeRequest {
    //         expression,
    //         old_lock_file: None,
    //     })
    //     .await?
    //     .lock_file;
    let x = evaluator
        .lock()
        .await
        .get_attributes(&GetAttributesRequest {
            expression: "{a=1;b=2;}".to_string(),
        })
        .await;

    eprintln!("YOURMOMYOURMOM {:?}", x);

    // let x = evaluator.lock();

    Ok(FileType::Flake {
        lock_file: format!("{:?}", x.unwrap()),
    })
}

fn safe_stringify_flake(source: &str) -> String {
    safe_stringify_opt(parse(source).expr().as_ref(), "/".as_ref())
}

#[cfg(test)]
mod test {
    use expect_test::expect;

    use crate::evaluator::{self, LockFlakeRequest};

    #[test_log::test(tokio::test)]
    async fn test_lock_flake() {
        let mut evaluator = evaluator::Evaluator::new().await;
        // let expr = r#"{ inputs.nixpkgs.url = "github:nixos/nixpkgs/67b8f2ca98f3bbc6f3b5f25cc28290111c921007"; }"#;
        let expr = r#"{ inputs.flake-utils.url = "github:numtide/flake-utils/11707dc2f618dd54ca8739b309ec4fc024de578b"; outputs = ({flake-utils ? null, ...}: (flake-utils.lib.aa)); }"#;
        let lock_file = evaluator
            .lock_flake(&LockFlakeRequest {
                expression: expr.to_owned(),
                old_lock_file: None,
            })
            .await
            .unwrap()
            .lock_file;
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
