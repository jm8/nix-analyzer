use std::process::Stdio;

use anyhow::{anyhow, Context, Result};
use lru::LruCache;
use serde::{Deserialize, Serialize};
use tokio::{
    io::{AsyncBufReadExt, AsyncWriteExt, BufReader},
    process::{ChildStdin, ChildStdout, Command},
};
use tracing::info;

#[derive(Serialize, Hash, PartialEq, Eq, Clone)]
pub struct GetAttributesRequest {
    pub expression: String,
}

pub type GetAttributesResponse = Vec<String>;

#[derive(Serialize, Hash, PartialEq, Eq, Clone)]
pub struct LockFlakeRequest {
    pub expression: String,
}

pub type LockFlakeResponse = String;

#[derive(Serialize)]
#[serde(tag = "method")]
#[serde(rename_all = "snake_case")]
enum Request<'a> {
    GetAttributes(&'a GetAttributesRequest),
    LockFlake(&'a LockFlakeRequest),
}

#[derive(Deserialize)]
#[serde(rename_all = "snake_case")]
enum Response<T> {
    Ok(T),
    Error(String),
}

#[derive(Debug)]
pub struct Evaluator {
    reader: BufReader<ChildStdout>,
    writer: ChildStdin,

    get_attributes_cache: LruCache<GetAttributesRequest, GetAttributesResponse>,
}

impl Evaluator {
    pub fn new() -> Self {
        let mut child = Command::new(concat!(env!("NIX_EVAL_SERVER"), "/bin/nix-eval-server"))
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .spawn()
            .context("failed to start nix-eval-server process")
            .unwrap();

        let writer = child.stdin.take().expect("Failed to open stdin");
        let reader = child.stdout.take().expect("Failed to open stdout");
        let reader = tokio::io::BufReader::new(reader);

        let get_attributes_cache = LruCache::new(20.try_into().unwrap());
        Self {
            reader,
            writer,
            get_attributes_cache,
        }
    }

    pub async fn get_attributes(
        &mut self,
        req: &GetAttributesRequest,
    ) -> Result<GetAttributesResponse> {
        if let Some(res) = self.get_attributes_cache.get(req) {
            info!("Get attributes cache hit");
            Ok(res.to_owned())
        } else {
            info!("Get attributes cache miss");
            let res: GetAttributesResponse = self.call(&Request::GetAttributes(req)).await?;
            self.get_attributes_cache.push(req.clone(), res.clone());
            Ok(res)
        }
    }

    pub async fn lock_flake(&mut self, req: &LockFlakeRequest) -> Result<LockFlakeResponse> {
        self.call(&Request::LockFlake(req)).await
    }

    async fn call<'a, Res>(&mut self, req: &'a Request<'a>) -> Result<Res>
    where
        Res: for<'b> Deserialize<'b>,
    {
        let request = serde_json::to_string(req).unwrap();
        info!(?request);
        let mut request = request.into_bytes();
        request.push(b'\n');
        self.writer.write_all(&request).await?;
        let mut buf = String::new();
        self.reader.read_line(&mut buf).await?;
        info!(buf);
        let res: Response<Res> = serde_json::from_str(&buf)?;
        match res {
            Response::Ok(res) => Ok(res),
            Response::Error(msg) => Err(anyhow!(msg)),
        }
    }
}
