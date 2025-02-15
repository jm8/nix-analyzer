#![allow(dead_code)]
#![feature(let_chains)]

mod completion;
mod evaluator;
mod flakes;
mod hover;
mod lambda_arg;
mod lsp;
mod safe_stringification;
mod syntax;

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
pub enum FileType {
    Package { nixpkgs_path: String },
    Custom { lambda_arg: String },
}

#[derive(Debug)]
pub struct File {
    contents: Rope,
    file_type: FileType,
}

#[derive(Debug)]
pub struct Analyzer {
    evaluator: Arc<Mutex<Evaluator>>,
    files: DashMap<PathBuf, File>,
}

impl Analyzer {
    fn new(evaluator: Arc<Mutex<Evaluator>>) -> Self {
        Self {
            evaluator,
            files: DashMap::new(),
        }
    }

    fn change_file(&self, path: &Path, contents: &str) {
        self.files
            .entry(path.into())
            .and_modify(|file| file.contents = contents.into())
            .or_insert_with(|| File {
                contents: contents.into(),
                file_type: FileType::Package {
                    nixpkgs_path: env!("nixpkgs").to_owned(),
                },
            });
    }

    fn get_diagnostics(&self, path: &Path) -> Result<Vec<Diagnostic>> {
        let _source = self.files.get(path).ok_or(anyhow!("file doesn't exist"))?;
        Ok(vec![])
    }

    async fn complete(&self, path: &Path, line: u32, col: u32) -> Result<Vec<CompletionItem>> {
        let file = self.files.get(path).ok_or(anyhow!("file doesn't exist"))?;
        let offset = file.contents.line_to_byte(line as usize) + col as usize;

        Ok(completion::complete(
            &file.contents.to_string(),
            &file.file_type,
            offset as u32,
            self.evaluator.clone(),
        )
        .await
        .unwrap_or_default())
    }

    fn hover(&self, path: &Path, line: u32, col: u32) -> Result<String> {
        let file = self.files.get(path).ok_or(anyhow!("file doesn't exist"))?;
        let offset = file.contents.line_to_byte(line as usize) + col as usize;

        Ok(hover::hover(&file.contents.to_string(), offset as u32)
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

    let evaluator = Arc::new(Mutex::new(Evaluator::new()));
    let (service, socket) = LspService::new(|client| Backend {
        client,
        analyzer: Analyzer::new(evaluator.clone()),
        evaluator,
    });
    Server::new(stdin, stdout, socket).serve(service).await;
}
