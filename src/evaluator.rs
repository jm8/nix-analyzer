use anyhow::{Context, Result};
use lru::LruCache;
use proto::AddVariableRequest;
use proto::{nix_eval_server_client::NixEvalServerClient, HoverRequest, HoverResponse};
use std::io::BufRead;
use std::process::{Child, Command, Stdio};
use tonic::transport::Channel;
use tracing::info;

pub mod proto {
    tonic::include_proto!("nix_eval_server");
}

#[derive(Debug, Clone, Hash, PartialEq, Eq)]
struct CacheKey {
    expression: String,
}

#[derive(Debug)]
pub struct Evaluator {
    client: NixEvalServerClient<Channel>,
    get_attributes_cache: LruCache<CacheKey, GetAttributesResponse>,
    hover_cache: LruCache<CacheKey, HoverResponse>,
    child: Child,
    var_number: u64,
}

pub use proto::{GetAttributesRequest, GetAttributesResponse, LockFlakeRequest, LockFlakeResponse};

impl Evaluator {
    pub async fn new() -> Self {
        let mut child = Command::new(concat!(env!("NIX_EVAL_SERVER"), "/bin/nix-eval-server"))
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .spawn()
            .context("failed to start nix-eval-server process")
            .unwrap();

        let reader = child.stdout.take().unwrap();
        let mut reader = std::io::BufReader::new(reader);
        let mut buf = String::new();
        reader
            .read_line(&mut buf)
            .expect("Failed to get port of nix-eval-server process");
        let port: u64 = buf
            .trim()
            .parse()
            .expect("Failed to get port of nix-eval-server process");

        info!(?port, "nix-eval-server port");

        let client = NixEvalServerClient::connect(format!("http://localhost:{}", port))
            .await
            .expect("Failed to connect to nix-eval-server");

        let get_attributes_cache = LruCache::new(40.try_into().unwrap());
        let hover_cache = LruCache::new(40.try_into().unwrap());
        let mut evaluator = Self {
            child,
            client,
            get_attributes_cache,
            hover_cache,
            var_number: 0,
        };

        evaluator
            .add_variable(&AddVariableRequest {
                name: "__nix_analyzer_get_flake_inputs".to_string(),
                expression: include_str!("get-flake-inputs.nix").to_string(),
            })
            .await
            .expect("Failed to initialize evaluator");

        evaluator
    }

    pub async fn get_attributes(
        &mut self,
        req: &GetAttributesRequest,
    ) -> Result<GetAttributesResponse> {
        let key = CacheKey {
            expression: req.expression.clone(),
        };
        if let Some(cached) = self.get_attributes_cache.get(&key) {
            return Ok(cached.clone());
        }
        let result = self
            .client
            .get_attributes(tonic::Request::new(req.clone()))
            .await?
            .into_inner();
        self.get_attributes_cache.push(key, result.clone());
        Ok(result)
    }

    pub async fn lock_flake(&mut self, req: &LockFlakeRequest) -> Result<LockFlakeResponse> {
        info!(?req, "Sending lock_flake request");
        let result = self
            .client
            .lock_flake(tonic::Request::new(req.clone()))
            .await?
            .into_inner();
        info!(?result, "lock_flake completed");
        Ok(result)
    }

    pub async fn hover(&mut self, req: &HoverRequest) -> Result<HoverResponse> {
        let key = CacheKey {
            expression: req.expression.clone(),
        };
        if let Some(cached) = self.hover_cache.get(&key) {
            return Ok(cached.clone());
        }
        let result = self
            .client
            .hover(tonic::Request::new(req.clone()))
            .await?
            .into_inner();
        self.hover_cache.push(key, result.clone());
        Ok(result)
    }

    pub async fn add_variable(&mut self, req: &AddVariableRequest) -> Result<()> {
        self.client
            .add_variable(tonic::Request::new(req.clone()))
            .await?
            .into_inner();
        Ok(())
    }

    pub async fn add_anonymous_variable(&mut self, expression: &str) -> Result<String> {
        self.var_number += 1;
        let name = format!("__nix_analyzer_{}", self.var_number);
        self.client
            .add_variable(tonic::Request::new(AddVariableRequest {
                name: name.to_string(),
                expression: expression.to_string(),
            }))
            .await?
            .into_inner();
        Ok(name)
    }
}

#[cfg(test)]
mod test {
    use expect_test::expect;

    use crate::evaluator::{Evaluator, GetAttributesRequest};

    #[tokio::test]
    async fn test_add_variable() {
        let mut evaluator = Evaluator::new().await;
        let name = evaluator
            .add_anonymous_variable("{ alpha = 1; beta = 2; gamma = 3;}")
            .await
            .unwrap();
        let actual = evaluator
            .get_attributes(&GetAttributesRequest { expression: name })
            .await
            .unwrap()
            .attributes;
        expect![[r#"
            [
                "alpha",
                "beta",
                "gamma",
            ]
        "#]]
        .assert_debug_eq(&actual);
    }
}

impl Drop for Evaluator {
    fn drop(&mut self) {
        _ = self.child.kill();
        _ = self.child.wait();
    }
}
