use std::{process::Stdio, time::Duration};

use anyhow::{anyhow, Context, Result};
use lru::LruCache;
use proto::nix_eval_server_client::NixEvalServerClient;
use tokio::{
    io::{AsyncBufReadExt, AsyncWriteExt, BufReader},
    process::{ChildStdin, ChildStdout, Command},
    time::{sleep, timeout},
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

        info!(?port, "nix-eval-server port");

        let client = NixEvalServerClient::connect(format!("http://localhost:{}", port))
            .await
            .expect("Failed to connect to nix-eval-server");

        // let get_attributes_cache = LruCache::new(20.try_into().unwrap());
        Self {
            client, // get_attributes_cache,
        }
    }

    #[tracing::instrument]
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
        // eprintln!("Get attributes Point A");
        // let future = self.client.get_attributes(tonic::Request::new(req.clone()));
        // info!("SETTING TIMEOUT");
        // let future = timeout(Duration::from_secs(5), future);
        // eprintln!("Get attributes Point B");
        // let return_value = future.await;
        // eprintln!("Get attributes Point C {:?}", return_value);
        info!("Starting to sleep");
        sleep(Duration::from_secs(5)).await;
        info!("Done sleeping");

        Ok(GetAttributesResponse {
            attributes: vec!["aaa".to_string(), "bbb".to_string()],
        })

        // Ok(return_value??.into_inner())
        //     self.get_attributes_cache.push(req.clone(), res.clone());
        //     Ok(res)
        // }
        // Ok(vec![])
    }

    #[tracing::instrument]
    pub async fn lock_flake(&mut self, req: &LockFlakeRequest) -> Result<LockFlakeResponse> {
        info!(?req, "Sending lock_flake request");
        let result = self
            .client
            .lock_flake(tonic::Request::new(req.clone()))
            .await?
            .into_inner();
        eprintln!("BBBBBBBBBBBBBB {:?}", result);
        Ok(result) // self.call(&Request::LockFlake(req)).await
    }
}
