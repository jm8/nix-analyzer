use crate::{
    evaluator::{Evaluator, LockFlakeRequest},
    file_types::FileType,
    safe_stringification::safe_stringify_opt,
    syntax::parse,
};
use anyhow::Result;

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
                "flake-utils": {
                  "inputs": {
                    "systems": "systems"
                  },
                  "locked": {
                    "lastModified": 1731533236,
                    "narHash": "sha256-l0KFg5HjrsfsO/JpG+r7fRrqm12kzFHyUHqHCVpMMbI=",
                    "owner": "numtide",
                    "repo": "flake-utils",
                    "rev": "11707dc2f618dd54ca8739b309ec4fc024de578b",
                    "type": "github"
                  },
                  "original": {
                    "owner": "numtide",
                    "repo": "flake-utils",
                    "rev": "11707dc2f618dd54ca8739b309ec4fc024de578b",
                    "type": "github"
                  }
                },
                "root": {
                  "inputs": {
                    "flake-utils": "flake-utils"
                  }
                },
                "systems": {
                  "locked": {
                    "lastModified": 1681028828,
                    "narHash": "sha256-Vy1rq5AaRuLzOxct8nz4T6wlgyUR7zLU309k9mBC768=",
                    "owner": "nix-systems",
                    "repo": "default",
                    "rev": "da67096a3b9bf56a91d16901293e51ba5b49a27e",
                    "type": "github"
                  },
                  "original": {
                    "owner": "nix-systems",
                    "repo": "default",
                    "type": "github"
                  }
                }
              },
              "root": "root",
              "version": 7
            }"#]]
        .assert_eq(&lock_file);
    }
}
