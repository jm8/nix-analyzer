use std::process::Stdio;

use anyhow::{anyhow, Context, Result};
use lru::LruCache;
use proto::nix_eval_server_client::NixEvalServerClient;
use tokio::{
    io::{AsyncBufReadExt, AsyncWriteExt, BufReader},
    process::{ChildStdin, ChildStdout, Command},
};
use tonic::transport::Channel;
use tracing::info;

pub mod proto {
    tonic::include_proto!("nix_eval_server");
}

#[derive(Debug)]
pub struct Evaluator {
    client: NixEvalServerClient<Channel>,
    // get_attributes_cache: LruCache<GetAttributesRequest, GetAttributesResponse>,
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

        let mut client = NixEvalServerClient::connect(format!("http://localhost:{}", port))
            .await
            .expect("Failed to connect to nix-eval-server");

        // let get_attributes_cache = LruCache::new(20.try_into().unwrap());
        Self {
            client, // get_attributes_cache,
        }
    }

    pub async fn get_attributes(
        &mut self,
        req: &GetAttributesRequest,
    ) -> Result<GetAttributesResponse> {
        // if let Some(res) = self.get_attributes_cache.get(req) {
        //     info!("Get attributes cache hit");
        //     Ok(res.to_owned())
        // } else {
        //     info!("Get attributes cache miss");
        //     let res: GetAttributesResponse = self.call(&Request::GetAttributes(req)).await?;
        Ok(self
            .client
            .get_attributes(tonic::Request::new(req.clone()))
            .await?
            .into_inner())
        //     self.get_attributes_cache.push(req.clone(), res.clone());
        //     Ok(res)
        // }
        // Ok(vec![])
    }

    pub async fn lock_flake(&mut self, req: &LockFlakeRequest) -> Result<LockFlakeResponse> {
        Ok(self
            .client
            .lock_flake(tonic::Request::new(req.clone()))
            .await?
            .into_inner())
        // self.call(&Request::LockFlake(req)).await
    }
}
