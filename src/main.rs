#![allow(dead_code)]
#![feature(let_chains)]

// mod completion;
// mod evaluation;
// mod hover;
// mod safe_stringification;

use anyhow::{anyhow, Result};
// use completion::complete;
use dashmap::DashMap;
use notification::PublishDiagnostics;
// use evaluation::eval_string;
// use notification::PublishDiagnostics;
use ropey::Rope;
use std::path::{Path, PathBuf};
use tower_lsp::jsonrpc;
use tower_lsp::lsp_types::*;
use tower_lsp::{Client, LanguageServer, LspService, Server};

#[derive(Debug)]
struct File {
    contents: Rope,
}

#[derive(Debug)]
struct Analyzer {
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
        let source = self.files.get(path).ok_or(anyhow!("file doesn't exist"))?;
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

        Ok("hello".to_owned())
        // Ok(hover::hover(&source.to_string(), offset as u32).unwrap_or_default())
    }
}

#[derive(Debug)]
struct Backend {
    client: Client,
    analyzer: Analyzer,
}

#[tower_lsp::async_trait]
impl LanguageServer for Backend {
    async fn initialize(&self, _: InitializeParams) -> jsonrpc::Result<InitializeResult> {
        Ok(InitializeResult {
            capabilities: ServerCapabilities {
                hover_provider: Some(HoverProviderCapability::Simple(true)),
                text_document_sync: Some(TextDocumentSyncCapability::Kind(
                    TextDocumentSyncKind::FULL,
                )),
                diagnostic_provider: Some(DiagnosticServerCapabilities::Options(
                    DiagnosticOptions {
                        identifier: None,
                        inter_file_dependencies: false,
                        workspace_diagnostics: false,
                        work_done_progress_options: WorkDoneProgressOptions {
                            work_done_progress: None,
                        },
                    },
                )),

                completion_provider: Some(CompletionOptions {
                    resolve_provider: None,
                    trigger_characters: Some(vec![".".into(), "/".into()]),
                    all_commit_characters: None,
                    work_done_progress_options: WorkDoneProgressOptions {
                        work_done_progress: None,
                    },
                    completion_item: None,
                }),
                ..Default::default()
            },
            server_info: None,
        })
    }

    async fn initialized(&self, _: InitializedParams) {
        self.client
            .log_message(MessageType::INFO, "server initialized!")
            .await;
    }

    async fn did_open(&self, params: DidOpenTextDocumentParams) {
        self.analyzer.change_file(
            Path::new(params.text_document.uri.path()),
            &params.text_document.text,
        );
        eprintln!("Did open {}", params.text_document.uri);
    }

    async fn did_change(&self, params: DidChangeTextDocumentParams) {
        for content_change in params.content_changes {
            self.analyzer.change_file(
                Path::new(params.text_document.uri.path()),
                &content_change.text,
            );
        }
        eprintln!("Did change {}", params.text_document.uri);
    }

    async fn did_save(&self, params: DidSaveTextDocumentParams) {
        let path = Path::new(params.text_document.uri.path());

        eprintln!("Did save {}", params.text_document.uri);

        let Ok(diagnostics) = self.analyzer.get_diagnostics(path).map_err(|_err| {
            eprintln!("{}", _err);
            jsonrpc::Error::internal_error()
        }) else {
            return;
        };

        eprintln!("Sending {} diagnostics", diagnostics.len());

        self.client
            .send_notification::<PublishDiagnostics>(PublishDiagnosticsParams {
                uri: Url::from_file_path(path).unwrap(),
                diagnostics,
                version: None,
            })
            .await;
    }

    async fn did_close(&self, params: DidCloseTextDocumentParams) {
        eprintln!("Did close {}", params.text_document.uri);
    }

    async fn hover(&self, params: HoverParams) -> jsonrpc::Result<Option<Hover>> {
        let path = Path::new(
            params
                .text_document_position_params
                .text_document
                .uri
                .path(),
        );

        let position = params.text_document_position_params.position;

        let md = self
            .analyzer
            .hover(path, position.line, position.character)
            .map_err(|_err| {
                eprintln!("{}", _err);
                jsonrpc::Error::internal_error()
            })?;

        fn markdown_hover(md: String) -> Hover {
            Hover {
                contents: HoverContents::Markup(MarkupContent {
                    kind: MarkupKind::Markdown,
                    value: md,
                }),
                range: None,
            }
        }

        Ok(Some(markdown_hover(md)))
    }

    async fn completion(
        &self,
        params: CompletionParams,
    ) -> jsonrpc::Result<Option<CompletionResponse>> {
        let path = Path::new(params.text_document_position.text_document.uri.path());
        let position = params.text_document_position.position;
        let items = self
            .analyzer
            .completion(path, position.line, position.character)
            .map_err(|err| {
                eprintln!("{}", err);
                jsonrpc::Error::internal_error()
            })?;

        Ok(Some(CompletionResponse::Array(items)))
    }

    async fn diagnostic(
        &self,
        params: DocumentDiagnosticParams,
    ) -> jsonrpc::Result<DocumentDiagnosticReportResult> {
        let path = Path::new(params.text_document.uri.path());

        let diagnostics = self.analyzer.get_diagnostics(path).map_err(|_err| {
            eprintln!("{}", _err);
            jsonrpc::Error::internal_error()
        })?;

        Ok(DocumentDiagnosticReportResult::Report(
            DocumentDiagnosticReport::Full(RelatedFullDocumentDiagnosticReport {
                related_documents: None,
                full_document_diagnostic_report: FullDocumentDiagnosticReport {
                    result_id: None,
                    items: diagnostics,
                },
            }),
        ))
    }

    async fn shutdown(&self) -> jsonrpc::Result<()> {
        Ok(())
    }
}

#[tokio::main]
async fn main() {
    let stdin = tokio::io::stdin();
    let stdout = tokio::io::stdout();

    let (service, socket) = LspService::new(|client| Backend {
        client,
        analyzer: Analyzer::new(),
    });
    Server::new(stdin, stdout, socket).serve(service).await;
}
