use crate::evaluator::Evaluator;
use crate::Analyzer;
use bmrng::RequestSender;
use std::path::Path;
use tower_lsp::jsonrpc;
use tower_lsp::lsp_types::notification::PublishDiagnostics;
use tower_lsp::lsp_types::{
    CompletionOptions, CompletionParams, CompletionResponse, DiagnosticOptions,
    DiagnosticServerCapabilities, DidChangeTextDocumentParams, DidCloseTextDocumentParams,
    DidOpenTextDocumentParams, DidSaveTextDocumentParams, DocumentDiagnosticParams,
    DocumentDiagnosticReport, DocumentDiagnosticReportResult, FullDocumentDiagnosticReport, Hover,
    HoverContents, HoverParams, HoverProviderCapability, InitializeParams, InitializeResult,
    InitializedParams, MarkupContent, MarkupKind, MessageType, PublishDiagnosticsParams,
    RelatedFullDocumentDiagnosticReport, ServerCapabilities, TextDocumentSyncCapability,
    TextDocumentSyncKind, Url, WorkDoneProgressOptions,
};
use tower_lsp::{Client, LanguageServer};

pub struct Backend {
    pub client: Client,
    pub analyzer: Analyzer,
    pub evaluator: Evaluator,
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
        let _path = Path::new(
            params
                .text_document_position_params
                .text_document
                .uri
                .path(),
        );

        let position = params.text_document_position_params.position;

        // let md = self
        //     .analyzer
        //     .hover(path, position.line, position.character)
        //     .map_err(|_err| {
        //         eprintln!("{}", _err);
        //         jsonrpc::Error::internal_error()
        //     })?;

        let line = position.line;

        let response = self.tx.send_receive(line as i32).await.unwrap();
        let md = format!("{}", response);

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
