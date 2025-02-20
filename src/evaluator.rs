use std::process::Stdio;

use anyhow::{Context, Result};
use lru::LruCache;
use proto::nix_eval_server_client::NixEvalServerClient;
use tokio::{io::AsyncBufReadExt, process::Command};
use tonic::transport::Channel;
use tracing::info;

pub mod proto {
    tonic::include_proto!("nix_eval_server");
}

#[derive(Debug, Clone, Hash, PartialEq, Eq)]
struct GetAttributesCacheKey {
    expression: String,
}

#[derive(Debug)]
pub struct Evaluator {
    client: NixEvalServerClient<Channel>,
    get_attributes_cache: LruCache<GetAttributesCacheKey, GetAttributesResponse>,
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
        let mut reader = tokio::io::BufReader::new(reader);
        let mut buf = String::new();
        reader
            .read_line(&mut buf)
            .await
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
        Self {
            client,
            get_attributes_cache,
        }
    }

    pub async fn get_attributes(
        &mut self,
        req: &GetAttributesRequest,
    ) -> Result<GetAttributesResponse> {
        let key = GetAttributesCacheKey {
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
}
