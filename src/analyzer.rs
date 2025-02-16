#![allow(dead_code)]

use crate::evaluator::Evaluator;
use crate::file_types::FileType;
use crate::schema::Schema;
use crate::{completion, hover};
use anyhow::{anyhow, Result};
use dashmap::DashMap;
use ropey::Rope;
use std::path::{Path, PathBuf};
use std::sync::Arc;
use tokio::sync::Mutex;
use tower_lsp::lsp_types::{CompletionItem, Diagnostic};

#[derive(Debug)]
pub struct File {
    contents: Rope,
    file_type: FileType,
}

#[derive(Debug)]
pub struct Analyzer {
    evaluator: Arc<Mutex<Evaluator>>,
    files: DashMap<PathBuf, File>,
    temp_nixos_module_schema: Arc<Schema>,
}

impl Analyzer {
    pub fn new(evaluator: Arc<Mutex<Evaluator>>) -> Self {
        Self {
            temp_nixos_module_schema: Arc::new(
                serde_json::from_str(include_str!("nixos_module_schema.json")).unwrap(),
            ),
            evaluator,
            files: DashMap::new(),
        }
    }

    pub fn change_file(&self, path: &Path, contents: &str) {
        self.files
            .entry(path.into())
            .and_modify(|file| file.contents = contents.into())
            .or_insert_with(|| File {
                contents: contents.into(),
                file_type: FileType::Package {
                    nixpkgs_path: env!("nixpkgs").to_owned(),
                    schema: self.temp_nixos_module_schema.clone(),
                },
            });
    }

    pub fn get_file_contents(&self, path: &Path) -> Result<Rope> {
        Ok(self
            .files
            .get(path)
            .ok_or_else(|| anyhow!("file doesn't exist"))?
            .contents
            .clone())
    }

    pub fn get_diagnostics(&self, path: &Path) -> Result<Vec<Diagnostic>> {
        let _source = self.files.get(path).ok_or(anyhow!("file doesn't exist"))?;
        Ok(vec![])
    }

    pub async fn complete(&self, path: &Path, line: u32, col: u32) -> Result<Vec<CompletionItem>> {
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

    pub fn hover(&self, path: &Path, line: u32, col: u32) -> Result<String> {
        let file = self.files.get(path).ok_or(anyhow!("file doesn't exist"))?;
        let offset = file.contents.line_to_byte(line as usize) + col as usize;

        Ok(hover::hover(&file.contents.to_string(), offset as u32)
            .map(|hover_result| hover_result.md)
            .unwrap_or_default())
    }
}
