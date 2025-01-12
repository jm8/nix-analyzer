#![allow(dead_code)]
#![feature(let_chains)]

mod evaluator;
mod hover;
mod lsp;
mod safe_stringification;
mod syntax;

mod nix_eval_server_capnp {
    include!(concat!(env!("OUT_DIR"), "/nix_eval_server_capnp.rs"));
}

use anyhow::{anyhow, Result};
use dashmap::DashMap;
use evaluator::Evaluator;
use lsp::Backend;
use ropey::Rope;
use std::path::{Path, PathBuf};
use std::sync::Arc;
use tokio::sync::Mutex;
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
        let offset = source.line_to_byte(line as usize) + col as usize;

        // Ok(complete(&source.to_string(), offset as u32).unwrap_or_default())
        Ok(vec![])
    }

    fn hover(&self, path: &Path, line: u32, col: u32) -> Result<String> {
        let source = self.files.get(path).ok_or(anyhow!("file doesn't exist"))?;
        let offset = source.line_to_byte(line as usize) + col as usize;

        Ok(hover::hover(&source.to_string(), offset as u32)
            .map(|hover_result| hover_result.md)
            .unwrap_or_default())
    }
}

struct Position {
    line: u32,
    col: u32,
    path: PathBuf,
}

struct HoverResult {
    md: String,
    position: Position,
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

    let (service, socket) = LspService::new(|client| Backend {
        client,
        analyzer: Analyzer::new(),
        evaluator: Arc::new(Mutex::new(Evaluator::new())),
    });
    Server::new(stdin, stdout, socket).serve(service).await;
}
