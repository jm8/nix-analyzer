#![allow(dead_code)]
#![feature(let_chains)]

// mod completion;
// mod evaluation;
// mod hover;
// mod safe_stringification;
mod evaluator;
mod lsp;
mod nix_eval_server_capnp {
    include!(concat!(env!("OUT_DIR"), "/nix_eval_server_capnp.rs"));
}

use anyhow::{anyhow, Context as _, Result};
use dashmap::DashMap;
use evaluator::Evaluator;
use lsp::Backend;
use ropey::Rope;
use std::path::{Path, PathBuf};
use std::process::Stdio;
use std::sync::Arc;
use tokio::io::{AsyncBufReadExt, AsyncWriteExt as _};
use tokio::process::Command;
use tokio::sync::Mutex;
use tokio::task::{spawn_local, LocalSet};
use tower_lsp::lsp_types::{CompletionItem, Diagnostic};
use tower_lsp::{LspService, Server};
use tracing_subscriber::EnvFilter;

#[derive(Debug)]
pub struct File {
    contents: Rope,
}

#[derive(Debug)]
pub struct Analyzer {
    files: DashMap<PathBuf, Rope>,
}

impl Analyzer {
    fn new() -> Self {
        Self {
            files: DashMap::new(),
        }
    }

    fn change_file(&self, path: &Path, contents: &str) {
        self.files.insert(path.into(), contents.into());
    }

    fn get_diagnostics(&self, path: &Path) -> Result<Vec<Diagnostic>> {
        let _source = self.files.get(path).ok_or(anyhow!("file doesn't exist"))?;
        Ok(vec![])
    }

    fn completion(&self, path: &Path, line: u32, col: u32) -> Result<Vec<CompletionItem>> {
        let source = self.files.get(path).ok_or(anyhow!("file doesn't exist"))?;
        let _offset = source.line_to_byte(line as usize) + col as usize;

        // Ok(complete(&source.to_string(), offset as u32).unwrap_or_default())
        Ok(vec![])
    }

    fn hover(&self, path: &Path, line: u32, col: u32) -> Result<String> {
        let source = self.files.get(path).ok_or(anyhow!("file doesn't exist"))?;
        let _offset = source.line_to_byte(line as usize) + col as usize;

        Ok("hello".to_owned())
        // Ok(hover::hover(&source.to_string(), offset as u32).unwrap_or_default())
    }
}

#[tokio::main(flavor = "current_thread")]
async fn main() {
    tracing_subscriber::fmt()
        .with_ansi(false)
        .with_writer(std::io::stderr)
        .with_env_filter(
            EnvFilter::try_from_default_env()
                .or_else(|_| EnvFilter::try_new("trace"))
                .unwrap(),
        )
        .init();

    let stdin = tokio::io::stdin();
    let stdout = tokio::io::stdout();

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
    let evaluator = Evaluator::new(reader, writer);

    let (service, socket) = LspService::new(|client| Backend {
        client,
        analyzer: Analyzer::new(),
        evaluator: Arc::new(Mutex::new(evaluator)),
    });
    Server::new(stdin, stdout, socket).serve(service).await;
}
