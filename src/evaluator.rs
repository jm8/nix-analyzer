use std::process::Stdio;

use anyhow::{anyhow, Context, Result};
use serde::{Deserialize, Serialize};
use tokio::{
    io::{AsyncBufReadExt, AsyncWriteExt, BufReader},
    process::{ChildStdin, ChildStdout, Command},
};
use tracing::info;

#[derive(Serialize)]
pub struct GetAttributesRequest {
    pub expression: String,
}

#[derive(Serialize)]
#[serde(tag = "method")]
#[serde(rename_all = "snake_case")]
enum Request<'a> {
    GetAttributes(&'a GetAttributesRequest),
}

#[derive(Deserialize)]
#[serde(rename_all = "snake_case")]
enum Response<T> {
    Ok(T),
    Error(String),
}

pub struct Evaluator {
    reader: BufReader<ChildStdout>,
    writer: ChildStdin,
}

impl Evaluator {
    pub fn new() -> Self {
        let mut child =
            Command::new(concat!(env!("NIX_EVAL_SERVER"), "/bin/nix-eval-server").to_owned())
                .stdin(Stdio::piped())
                .stdout(Stdio::piped())
                .spawn()
                .context("failed to start nix-eval-server process")
                .unwrap();

        let writer = child.stdin.take().expect("Failed to open stdin");
        let reader = child.stdout.take().expect("Failed to open stdout");
        let reader = tokio::io::BufReader::new(reader);
        Self { reader, writer }
    }

    pub async fn get_attributes(&mut self, req: &GetAttributesRequest) -> Result<Vec<String>> {
        self.call(&Request::GetAttributes(req)).await
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
