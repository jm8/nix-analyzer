use std::{path::Path, sync::Arc};

use lsp_server::{Connection, ExtractError, Message, Notification, Request, RequestId, Response};
use tokio::{runtime, sync::Mutex};
// use crate::evaluator::Evaluator;
// use crate::Analyzer;
// use std::path::Path;
// use std::sync::Arc;
// use tokio::sync::Mutex;
// use tower_lsp::jsonrpc::{self};
use anyhow::{bail, Result};
// use tower_lsp::lsp_types::notification::PublishDiagnostics;
use lsp_types::{
    notification::{DidChangeTextDocument, DidOpenTextDocument},
    request::{Completion, GotoDefinition, HoverRequest},
    CompletionItem, CompletionOptions, CompletionParams, CompletionResponse, DiagnosticOptions,
    DiagnosticServerCapabilities, DidChangeTextDocumentParams, DidCloseTextDocumentParams,
    DidOpenTextDocumentParams, DidSaveTextDocumentParams, DocumentDiagnosticParams,
    DocumentDiagnosticReport, DocumentDiagnosticReportResult, DocumentFormattingParams,
    FullDocumentDiagnosticReport, GotoDefinitionResponse, Hover, HoverContents, HoverParams,
    HoverProviderCapability, InitializeParams, InitializeResult, InitializedParams, MarkupContent,
    MarkupKind, OneOf, Position, PublishDiagnosticsParams, Range,
    RelatedFullDocumentDiagnosticReport, ServerCapabilities, TextDocumentSyncCapability,
    TextDocumentSyncKind, TextEdit, WorkDoneProgressOptions,
};
use tracing::{error, info, warn};

use crate::{
    analyzer::Analyzer, completion::complete, evaluator::Evaluator, file_types::FileInfo,
    TOKIO_RUNTIME,
};
// use tower_lsp::{Client, LanguageServer};
// use tracing::{error, info};

// pub struct Backend {
//     pub client: Client,
//     pub analyzer: Analyzer,
//     pub evaluator: &mut Evaluator,
// }

pub fn capabilities() -> ServerCapabilities {
    ServerCapabilities {
        hover_provider: Some(HoverProviderCapability::Simple(true)),
        text_document_sync: Some(TextDocumentSyncCapability::Kind(TextDocumentSyncKind::FULL)),
        // diagnostic_provider: Some(DiagnosticServerCapabilities::Options(DiagnosticOptions {
        //     identifier: None,
        //     inter_file_dependencies: false,
        //     workspace_diagnostics: false,
        //     work_done_progress_options: WorkDoneProgressOptions {
        //         work_done_progress: None,
        //     },
        // })),
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
    }
}

pub fn main_loop(connection: Connection, params: serde_json::Value) -> Result<()> {
    let _params: InitializeParams = serde_json::from_value(params).unwrap();

    let evaluator = TOKIO_RUNTIME.block_on(async { Evaluator::new().await });
    let mut analyzer = Analyzer::new(evaluator);

    info!("Welcome to nix-analyzer!");
    for msg in &connection.receiver {
        match msg {
            Message::Request(req) => {
                if connection.handle_shutdown(&req)? {
                    return Ok(());
                }

                let id = req.id.clone();
                match handle_request(&mut analyzer, req) {
                    Ok(resp) => {
                        connection.sender.send(resp.into())?;
                    }
                    Err(err) => connection
                        .sender
                        .send(Response::new_err(id, 0, err.to_string()).into())?,
                }
            }
            Message::Response(_) => {}
            Message::Notification(not) => {
                handle_notification(&mut analyzer, not)?;
            }
        }
    }
    Ok(())
}

fn handle_request(analyzer: &mut Analyzer, req: Request) -> Result<Response> {
    let req = match cast::<HoverRequest>(req) {
        Ok((id, params)) => {
            let path = Path::new(
                params
                    .text_document_position_params
                    .text_document
                    .uri
                    .path()
                    .as_str()
                    .into(),
            );

            let position = params.text_document_position_params.position;

            // let md = self
            //     .analyzer
            //     .hover(path, position.line - 1, position.character - 1)
            //     .map_err(|err| {
            //         error!(?err, "Error getting hover");
            //         jsonrpc::Error::internal_error()
            //     })?;

            // let md = "Test".to_string();
            let md = analyzer.get_file_contents(path)?.to_string();

            fn markdown_hover(md: String) -> Hover {
                Hover {
                    contents: HoverContents::Markup(MarkupContent {
                        kind: MarkupKind::Markdown,
                        value: md,
                    }),
                    range: None,
                }
            }

            return Ok(Response::new_ok(id, markdown_hover(md)));
        }
        Err(err @ ExtractError::JsonError { .. }) => panic!("{err:?}"),
        Err(ExtractError::MethodMismatch(req)) => req,
    };
    let req = match cast::<Completion>(req) {
        Ok((id, params)) => {
            let path = Path::new(
                params
                    .text_document_position
                    .text_document
                    .uri
                    .path()
                    .as_str()
                    .into(),
            );

            let position = params.text_document_position.position;

            let result =
                TOKIO_RUNTIME.block_on(analyzer.complete(path, position.line, position.character));

            // let result = analyzer.

            return Ok(Response::new_ok(
                id,
                Some(CompletionResponse::Array(result.unwrap_or_default())),
            ));
        }
        Err(err @ ExtractError::JsonError { .. }) => panic!("{err:?}"),
        Err(ExtractError::MethodMismatch(req)) => req,
    };

    // if let Ok((id, params)) = cast::<Completion>(req) {

    //     return Ok(Response::new_ok(
    //         id,
    //         CompletionResponse::Array(vec![CompletionItem {
    //             label: "aaa".to_string(),
    //             ..Default::default()
    //         }]),
    //     ));
    // }
    bail!("Unhandled request");
}

fn handle_notification(analyzer: &mut Analyzer, not: Notification) -> Result<()> {
    let not = match cast_not::<DidOpenTextDocument>(not) {
        Ok(params) => {
            let path = Path::new(params.text_document.uri.path().as_str().into());

            info!(?path, "Opened document");

            TOKIO_RUNTIME.block_on(analyzer.change_file(path, &params.text_document.text));

            return Ok(());
        }
        Err(err @ ExtractError::JsonError { .. }) => panic!("{err:?}"),
        Err(ExtractError::MethodMismatch(not)) => not,
    };
    let not = match cast_not::<DidChangeTextDocument>(not) {
        Ok(params) => {
            let path = Path::new(params.text_document.uri.path().as_str().into());

            info!(?path, "Change document");

            let mut contents = analyzer
                .get_file_contents(path)
                .unwrap_or_default()
                .to_string();
            for change in params.content_changes {
                contents = change.text;
            }
            TOKIO_RUNTIME.block_on(analyzer.change_file(path, &contents));

            return Ok(());
        }
        Err(err @ ExtractError::JsonError { .. }) => panic!("{err:?}"),
        Err(ExtractError::MethodMismatch(not)) => not,
    };

    // if let Ok((id, params)) = cast::<Completion>(req) {

    //     return Ok(Response::new_ok(
    //         id,
    //         CompletionResponse::Array(vec![CompletionItem {
    //             label: "aaa".to_string(),
    //             ..Default::default()
    //         }]),
    //     ));
    // }
    warn!(?not, "Unhandled notification");
    Ok(())
}

fn cast<R>(req: Request) -> Result<(RequestId, R::Params), ExtractError<Request>>
where
    R: lsp_types::request::Request,
    R::Params: serde::de::DeserializeOwned,
{
    req.extract(R::METHOD)
}

fn cast_not<N>(not: Notification) -> Result<N::Params, ExtractError<Notification>>
where
    N: lsp_types::notification::Notification,
    N::Params: serde::de::DeserializeOwned,
{
    not.extract(N::METHOD)
}

// #[tower_lsp::async_trait]

// impl LanguageServer for Backend {
//     async fn initialize(&self, _: InitializeParams) -> jsonrpc::Result<InitializeResult> {
//         Ok(InitializeResult {
//             capabilities: ServerCapabilities {
//                 hover_provider: Some(HoverProviderCapability::Simple(true)),
//                 text_document_sync: Some(TextDocumentSyncCapability::Kind(
//                     TextDocumentSyncKind::FULL,
//                 )),
//                 diagnostic_provider: Some(DiagnosticServerCapabilities::Options(
//                     DiagnosticOptions {
//                         identifier: None,
//                         inter_file_dependencies: false,
//                         workspace_diagnostics: false,
//                         work_done_progress_options: WorkDoneProgressOptions {
//                             work_done_progress: None,
//                         },
//                     },
//                 )),
//                 completion_provider: Some(CompletionOptions {
//                     resolve_provider: None,
//                     trigger_characters: Some(vec![".".into(), "/".into()]),
//                     all_commit_characters: None,
//                     work_done_progress_options: WorkDoneProgressOptions {
//                         work_done_progress: None,
//                     },
//                     completion_item: None,
//                 }),
//                 document_formatting_provider: Some(OneOf::Left(true)),
//                 ..Default::default()
//             },
//             server_info: None,
//         })
//     }

//     async fn initialized(&self, _: InitializedParams) {
//         info!("server initialized")
//     }

//     async fn did_open(&self, params: DidOpenTextDocumentParams) {
//         self.analyzer
//             .change_file(
//                 Path::new(params.text_document.uri.path()),
//                 &params.text_document.text,
//             )
//             .await;
//         eprintln!("Did open {}", params.text_document.uri);
//     }

//     async fn did_change(&self, params: DidChangeTextDocumentParams) {
//         for content_change in params.content_changes {
//             self.analyzer
//                 .change_file(
//                     Path::new(params.text_document.uri.path()),
//                     &content_change.text,
//                 )
//                 .await;
//         }
//         eprintln!("Did change {}", params.text_document.uri);
//     }

//     async fn did_save(&self, params: DidSaveTextDocumentParams) {
//         let path = Path::new(params.text_document.uri.path());

//         eprintln!("Did save {}", params.text_document.uri);

//         let Ok(diagnostics) = self.analyzer.get_diagnostics(path).map_err(|err| {
//             error!(?err, "Error getting diagnostics");
//             jsonrpc::Error::internal_error()
//         }) else {
//             return;
//         };

//         eprintln!("Sending {} diagnostics", diagnostics.len());

//         self.client
//             .send_notification::<PublishDiagnostics>(PublishDiagnosticsParams {
//                 uri: Url::from_file_path(path).unwrap(),
//                 diagnostics,
//                 version: None,
//             })
//             .await;
//     }

//     async fn did_close(&self, params: DidCloseTextDocumentParams) {
//         eprintln!("Did close {}", params.text_document.uri);
//     }

//     async fn hover(&self, params: HoverParams) -> jsonrpc::Result<Option<Hover>> {
//         let path = Path::new(
//             params
//                 .text_document_position_params
//                 .text_document
//                 .uri
//                 .path(),
//         );

//         let position = params.text_document_position_params.position;

//         let md = self
//             .analyzer
//             .hover(path, position.line - 1, position.character - 1)
//             .map_err(|err| {
//                 error!(?err, "Error getting hover");
//                 jsonrpc::Error::internal_error()
//             })?;

//         fn markdown_hover(md: String) -> Hover {
//             Hover {
//                 contents: HoverContents::Markup(MarkupContent {
//                     kind: MarkupKind::Markdown,
//                     value: md,
//                 }),
//                 range: None,
//             }
//         }

//         Ok(Some(markdown_hover(md)))
//     }

//     async fn completion(
//         &self,
//         params: CompletionParams,
//     ) -> jsonrpc::Result<Option<CompletionResponse>> {
//         let path = Path::new(params.text_document_position.text_document.uri.path());
//         let position = params.text_document_position.position;
//         let items = self
//             .analyzer
//             .complete(path, position.line, position.character)
//             .await
//             .map_err(|err| {
//                 error!(?err, "Error getting completion");
//                 jsonrpc::Error::internal_error()
//             })?;

//         Ok(Some(CompletionResponse::Array(items)))
//     }

//     async fn diagnostic(
//         &self,
//         params: DocumentDiagnosticParams,
//     ) -> jsonrpc::Result<DocumentDiagnosticReportResult> {
//         let path = Path::new(params.text_document.uri.path());

//         let diagnostics = self.analyzer.get_diagnostics(path).map_err(|err| {
//             error!(?err, "Error getting diagnostics");
//             jsonrpc::Error::internal_error()
//         })?;

//         Ok(DocumentDiagnosticReportResult::Report(
//             DocumentDiagnosticReport::Full(RelatedFullDocumentDiagnosticReport {
//                 related_documents: None,
//                 full_document_diagnostic_report: FullDocumentDiagnosticReport {
//                     result_id: None,
//                     items: diagnostics,
//                 },
//             }),
//         ))
//     }

//     async fn shutdown(&self) -> jsonrpc::Result<()> {
//         Ok(())
//     }

//     async fn formatting(
//         &self,
//         params: DocumentFormattingParams,
//     ) -> jsonrpc::Result<Option<Vec<TextEdit>>> {
//         let new_text = self
//             .analyzer
//             .format(Path::new(params.text_document.uri.path()))
//             .await
//             .map_err(|err| {
//                 error!(?err, "failed to format");
//                 jsonrpc::Error::internal_error()
//             })?;
//         let range = Range {
//             start: Position {
//                 line: 0,
//                 character: 0,
//             },
//             end: Position {
//                 line: 999999,
//                 character: 0,
//             },
//         };
//         Ok(Some(vec![TextEdit { range, new_text }]))
//     }
// }
