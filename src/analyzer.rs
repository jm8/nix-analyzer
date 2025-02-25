#![allow(dead_code)]

use crate::evaluator::Evaluator;
use crate::fetcher::{FetcherInput, FetcherOutput};
use crate::file_types::{init_file_info, FileInfo, FileType, LockedFlake};
use crate::flakes::get_flake_inputs;
use crate::safe_stringification::safe_stringify_flake;
use crate::schema::Schema;
use crate::syntax::parse;
use crate::{completion, hover};
use anyhow::{anyhow, bail, Context, Result};
use crossbeam::channel::{Receiver, Sender};
use lsp_types::{CompletionItem, Diagnostic};
use ropey::Rope;
use std::collections::HashMap;
use std::io::Read;
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
    pub fetcher_input_send: Sender<FetcherInput>,
    pub fetcher_output_recv: Receiver<FetcherOutput>,
}

impl Analyzer {
    pub fn new(
        evaluator: Evaluator,
        fetcher_input_send: Sender<FetcherInput>,
        fetcher_output_recv: Receiver<FetcherOutput>,
    ) -> Self {
        Self {
            temp_nixos_module_schema: Arc::new(
                serde_json::from_str(include_str!("nixos_module_schema.json")).unwrap(),
            ),
            evaluator,
            files: HashMap::new(),
            fetcher_input_send,
            fetcher_output_recv,
        }
    }

    pub fn change_file(&mut self, path: &Path, contents: &str) {
        if let Some(x) = self.files.get_mut(path) {
            x.contents = contents.into();
            return;
        }
        let mut file = File {
            contents: contents.into(),
            file_info: init_file_info(path, contents, self.temp_nixos_module_schema.clone()),
        };
        if let FileType::Flake {
            locked: LockedFlake::None,
        } = file.file_info.file_type
        {
            let old_lock_file = self
                .get_file_contents(&path.parent().unwrap().join("flake.lock"))
                .map(|s| s.to_string())
                .ok();
            self.fetcher_input_send
                .send(FetcherInput {
                    path: path.to_path_buf(),
                    source: safe_stringify_flake(
                        parse(contents).expr().as_ref(),
                        path.parent().unwrap(),
                    ),
                    old_lock_file,
                })
                .unwrap();
            file.file_info.file_type = FileType::Flake {
                locked: LockedFlake::Pending,
            };
        }
        self.files.insert(path.to_owned(), file);
    }

    pub fn get_file_contents(&mut self, path: &Path) -> Result<Rope> {
        if let Some(contents) = self.files.get(path).map(|file| file.contents.clone()) {
            return Ok(contents);
        }

        let contents = std::fs::read_to_string(path)?;
        self.change_file(path, &contents);
        Ok(Rope::from(contents))
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

    pub async fn hover(&mut self, path: &Path, line: u32, col: u32) -> Result<Option<String>> {
        let file = self.files.get(path).ok_or(anyhow!("file doesn't exist"))?;
        let offset = file.contents.line_to_byte(line as usize) + col as usize;

        Ok(hover::hover(
            &file.contents.to_string(),
            &file.file_info,
            offset as u32,
            &mut self.evaluator,
        )
        .await
        .map(|hover_result| hover_result.md)
        .ok())
    }

    pub async fn format(&mut self, path: &Path) -> Result<String> {
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

    pub async fn process_fetcher_output(&mut self) -> Result<()> {
        if let Ok(output) = self.fetcher_output_recv.try_recv() {
            let inputs = get_flake_inputs(&mut self.evaluator, &output.lock_file).await?;
            self.files
                .get_mut(&output.path)
                .into_iter()
                .for_each(|file| {
                    file.file_info.file_type = FileType::Flake {
                        locked: LockedFlake::Locked {
                            lock_file: output.lock_file.clone(),
                            inputs: inputs.clone(),
                        },
                    }
                });
        }
        Ok(())
    }
}
