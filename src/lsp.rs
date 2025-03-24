use anyhow::{bail, Result};
use lsp_server::{Connection, ExtractError, Message, Notification, Request, RequestId, Response};
use lsp_types::{
    notification::{DidChangeTextDocument, DidOpenTextDocument},
    request::{Completion, Formatting, GotoDefinition, HoverRequest},
    CompletionOptions, CompletionResponse, GotoDefinitionResponse, Hover, HoverContents,
    HoverProviderCapability, InitializeParams, Location, MarkupContent, MarkupKind, OneOf,
    Position, Range, ServerCapabilities, TextDocumentSyncCapability, TextDocumentSyncKind,
    TextEdit, Uri, WorkDoneProgressOptions,
};
use std::{path::Path, str::FromStr};
use tracing::{error, info, warn};

use crate::{
    analyzer::Analyzer, evaluator::Evaluator, fetcher::NonBlockingFetcher, hover::HoverResult,
    TOKIO_RUNTIME,
};

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
        definition_provider: Some(OneOf::Left(true)),
        ..Default::default()
    }
}
pub fn main_loop(connection: Connection, params: serde_json::Value) -> Result<()> {
    let _params: InitializeParams = serde_json::from_value(params).unwrap();

    let evaluator = TOKIO_RUNTIME.block_on(async { Evaluator::new().await });
    let mut analyzer = Analyzer::new(
        evaluator,
        Box::new(NonBlockingFetcher::new(connection.sender.clone())),
    );

    info!("Welcome to nix-analyzer!");
    for msg in &connection.receiver {
        TOKIO_RUNTIME.block_on(analyzer.process_fetcher_output());
        match msg {
            Message::Request(req) => {
                if connection.handle_shutdown(&req)? {
                    // let _ = fetcher_thread.join();
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
        TOKIO_RUNTIME.block_on(analyzer.process_fetcher_output());
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
                    .as_str(),
            );

            let position = params.text_document_position_params.position;

            let md = match TOKIO_RUNTIME.block_on(analyzer.hover(
                path,
                position.line,
                position.character,
            ))? {
                Some(hover) => hover.md,
                None => return Ok(Response::new_ok(id, None::<Hover>)),
            };

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
    let req = match cast::<GotoDefinition>(req) {
        Ok((id, params)) => {
            let path = Path::new(
                params
                    .text_document_position_params
                    .text_document
                    .uri
                    .path()
                    .as_str(),
            );

            let position = params.text_document_position_params.position;

            let position = match TOKIO_RUNTIME.block_on(analyzer.hover(
                path,
                position.line,
                position.character,
            ))? {
                Some(HoverResult {
                    md: _,
                    position: Some(position),
                }) => position,
                _ => return Ok(Response::new_ok(id, None::<GotoDefinitionResponse>)),
            };

            return Ok(Response::new_ok(
                id,
                GotoDefinitionResponse::Scalar(Location {
                    uri: Uri::from_str(&format!("file://{}", position.path.display()))?,
                    range: Range {
                        start: Position {
                            line: position.line,
                            character: position.col,
                        },
                        end: Position {
                            line: position.line,
                            character: position.col + 1,
                        },
                    },
                }),
            ));
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
                    .as_str(),
            );

            let position = params.text_document_position.position;

            let result =
                TOKIO_RUNTIME.block_on(analyzer.complete(path, position.line, position.character));

            return Ok(Response::new_ok(
                id,
                Some(CompletionResponse::Array(result.unwrap_or_default())),
            ));
        }
        Err(err @ ExtractError::JsonError { .. }) => panic!("{err:?}"),
        Err(ExtractError::MethodMismatch(req)) => req,
    };
    let _req = match cast::<Formatting>(req) {
        Ok((id, params)) => {
            let path = Path::new(params.text_document.uri.path().as_str());

            let new_text = match TOKIO_RUNTIME.block_on(analyzer.format(path)) {
                Ok(new_text) => new_text,
                Err(err) => {
                    error!(?err, "Failed to format");
                    return Ok(Response::new_ok(id, None::<Vec<TextEdit>>));
                }
            };

            let range = Range {
                start: Position {
                    line: 0,
                    character: 0,
                },
                end: Position {
                    line: 99999999,
                    character: 0,
                },
            };

            return Ok(Response::new_ok(
                id,
                Some(vec![TextEdit { range, new_text }]),
            ));
        }
        Err(err @ ExtractError::JsonError { .. }) => panic!("{err:?}"),
        Err(ExtractError::MethodMismatch(req)) => req,
    };

    bail!("Unhandled request");
}

fn handle_notification(analyzer: &mut Analyzer, not: Notification) -> Result<()> {
    let not = match cast_not::<DidOpenTextDocument>(not) {
        Ok(params) => {
            let path = Path::new(params.text_document.uri.path().as_str());

            info!(?path, "Opened document");

            TOKIO_RUNTIME.block_on(analyzer.change_file(path, &params.text_document.text));

            return Ok(());
        }
        Err(err @ ExtractError::JsonError { .. }) => panic!("{err:?}"),
        Err(ExtractError::MethodMismatch(not)) => not,
    };
    let not = match cast_not::<DidChangeTextDocument>(not) {
        Ok(params) => {
            let path = Path::new(params.text_document.uri.path().as_str());

            info!(?path, "Change document");

            let mut contents = TOKIO_RUNTIME
                .block_on(analyzer.get_file_contents(path))
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
