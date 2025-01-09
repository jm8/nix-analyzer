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

use anyhow::{anyhow, Result};
use dashmap::DashMap;
use evaluator::{evaluator_thread, Evaluator, EvaluatorRequest, EvaluatorResponse};
use lsp::Backend;
use ropey::Rope;
use std::path::{Path, PathBuf};
use tokio::task::{spawn_local, LocalSet};
use tower_lsp::lsp_types::{CompletionItem, Diagnostic};
use tower_lsp::{LspService, Server};
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
    let stdin = tokio::io::stdin();
    let stdout = tokio::io::stdout();

    let buffer_size = 8;
    let (tx, rx) = bmrng::channel::<EvaluatorRequest, EvaluatorResponse>(buffer_size);

    LocalSet::new()
        .run_until(async {
            spawn_local(evaluator_thread(rx));
            let (service, socket) = LspService::new(|client| Backend {
                client,
                evaluator: Evaluator { tx },
                analyzer: Analyzer::new(),
            });
            Server::new(stdin, stdout, socket).serve(service).await;
        })
        .await;
}
