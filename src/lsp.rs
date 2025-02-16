use crate::evaluator::Evaluator;
use crate::Analyzer;
use std::path::Path;
use std::process::Stdio;
use std::sync::Arc;
use tokio::process::Command;
use tokio::sync::Mutex;
use tower_lsp::jsonrpc::{self};
use tower_lsp::lsp_types::notification::PublishDiagnostics;
use tower_lsp::lsp_types::{
    CompletionOptions, CompletionParams, CompletionResponse, DiagnosticOptions,
    DiagnosticServerCapabilities, DidChangeTextDocumentParams, DidCloseTextDocumentParams,
    DidOpenTextDocumentParams, DidSaveTextDocumentParams, DocumentDiagnosticParams,
    DocumentDiagnosticReport, DocumentDiagnosticReportResult, DocumentFormattingParams,
    FullDocumentDiagnosticReport, Hover, HoverContents, HoverParams, HoverProviderCapability,
    InitializeParams, InitializeResult, InitializedParams, MarkupContent, MarkupKind, OneOf,
    Position, PublishDiagnosticsParams, Range, RelatedFullDocumentDiagnosticReport,
    ServerCapabilities, TextDocumentSyncCapability, TextDocumentSyncKind, TextEdit, Url,
    WorkDoneProgressOptions,
};
use tower_lsp::{Client, LanguageServer};
use tracing::info;

pub struct Backend {
    pub client: Client,
    pub analyzer: Analyzer,
    pub evaluator: Arc<Mutex<Evaluator>>,
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
                document_formatting_provider: Some(OneOf::Left(true)),
                ..Default::default()
            },
            server_info: None,
        })
    }

    async fn initialized(&self, _: InitializedParams) {
        info!("server initialized")
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
            .hover(path, position.line - 1, position.character - 1)
            .map_err(|_| jsonrpc::Error::internal_error())?;

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
            .complete(path, position.line, position.character)
            .await
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

    async fn formatting(
        &self,
        params: DocumentFormattingParams,
    ) -> jsonrpc::Result<Option<Vec<TextEdit>>> {
        let source = self
            .analyzer
            .files
            .get(Path::new(params.text_document.uri.path()))
            .ok_or_else(jsonrpc::Error::internal_error)?
            .contents
            .to_string()
            .into_bytes();
        let mut child = Command::new("alejandra")
            .args(["-q", "-"])
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .stderr(Stdio::piped())
            .spawn()
            .map_err(|_| jsonrpc::Error::internal_error())?;
        let mut stdin = child.stdin.take().unwrap();
        tokio::io::copy(&mut source.as_ref(), &mut stdin)
            .await
            .map_err(|_| jsonrpc::Error::internal_error())?;
        // Close stdin
        drop(stdin);
        let output = child
            .wait_with_output()
            .await
            .map_err(|_| jsonrpc::Error::internal_error())?;
        if !output.status.success() {
            return Err(jsonrpc::Error::internal_error());
        }
        let new_text =
            String::from_utf8(output.stdout).map_err(|_| jsonrpc::Error::internal_error())?;
        let range = Range {
            start: Position {
                line: 0,
                character: 0,
            },
            end: Position {
                line: 999999,
                character: 0,
            },
        };
        Ok(Some(vec![TextEdit { range, new_text }]))
    }
}
