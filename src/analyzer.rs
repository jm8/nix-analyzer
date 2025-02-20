#![allow(dead_code)]

use crate::evaluator::Evaluator;
use crate::file_types::{get_file_info, FileInfo};
use crate::schema::Schema;
use crate::{completion, hover};
use anyhow::{anyhow, bail, Context, Result};
use lsp_types::{CompletionItem, Diagnostic};
use ropey::Rope;
use std::collections::HashMap;
use std::path::{Path, PathBuf};
use std::process::Stdio;
use std::sync::Arc;
use tokio::process::Command;

#[derive(Debug)]
pub struct File {
    contents: Rope,
    file_info: FileInfo,
}

#[derive(Debug)]
pub struct Analyzer {
    pub evaluator: Evaluator,
    pub files: HashMap<PathBuf, File>,
    pub temp_nixos_module_schema: Arc<Schema>,
}

impl Analyzer {
    pub fn new(evaluator: Evaluator) -> Self {
        Self {
            temp_nixos_module_schema: Arc::new(
                serde_json::from_str(include_str!("nixos_module_schema.json")).unwrap(),
            ),
            evaluator,
            files: HashMap::new(),
        }
    }

    pub async fn change_file(&mut self, path: &Path, contents: &str) {
        if let Some(x) = self.files.get_mut(path) {
            x.contents = contents.into();
            return;
        }
        let file = File {
            contents: contents.into(),
            file_info: get_file_info(
                &mut self.evaluator,
                path,
                contents,
                self.temp_nixos_module_schema.clone(),
            )
            .await,
        };
        self.files.insert(path.to_owned(), file);
    }

    pub fn get_file_contents(&self, path: &Path) -> Result<Rope> {
        Ok(self
            .files
            .get(path)
            .ok_or_else(|| anyhow!("file doesn't exist"))?
            .contents
            .clone())
    }

    pub fn get_diagnostics(&self, _path: &Path) -> Result<Vec<Diagnostic>> {
        // let _source = self.files.get(path).ok_or(anyhow!("file doesn't exist"))?;
        Ok(vec![])
    }

    pub async fn complete(
        &mut self,
        path: &Path,
        line: u32,
        col: u32,
    ) -> Result<Vec<CompletionItem>> {
        let file = self.files.get(path).ok_or(anyhow!("file doesn't exist"))?;
        let offset = file.contents.line_to_byte(line as usize) + col as usize;

        Ok(completion::complete(
            &file.contents.to_string(),
            &file.file_info,
            offset as u32,
            &mut self.evaluator,
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

    pub async fn format(&self, path: &Path) -> Result<String> {
        let source = self.get_file_contents(path)?.to_string().into_bytes();
        let mut child = Command::new(env!("ALEJANDRA"))
            .args(["-q", "-"])
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .stderr(Stdio::piped())
            .spawn()
            .context("failed to start formatter process")?;
        let mut stdin = child.stdin.take().unwrap();
        tokio::io::copy(&mut source.as_ref(), &mut stdin)
            .await
            .context("failed to write to formatter process")?;
        // Close stdin
        drop(stdin);
        let output = child
            .wait_with_output()
            .await
            .context("failed to wait for formatter process")?;
        if !output.status.success() {
            bail!("formatter process exited unsuccesfully")
        }
        let new_text =
            String::from_utf8(output.stdout).context("formatter output contains invalid utf-8")?;
        Ok(new_text)
    }
}
