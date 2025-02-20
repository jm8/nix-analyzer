use anyhow::{bail, Result};
use crossbeam::channel::Sender;
use lsp_server::{Connection, ExtractError, Message, Notification, Request, RequestId, Response};
use lsp_types::{
    notification::{
        DidChangeTextDocument, DidOpenTextDocument, Notification as _, Progress, ShowMessage,
    },
    request::{self, Completion, Formatting, HoverRequest, Request as _, WorkDoneProgressCreate},
    CompletionOptions, CompletionResponse, Hover, HoverContents, HoverProviderCapability,
    InitializeParams, MarkupContent, MarkupKind, MessageType, NumberOrString, OneOf, Position,
    ProgressParams, ProgressParamsValue, Range, ServerCapabilities, ShowMessageParams,
    TextDocumentSyncCapability, TextDocumentSyncKind, TextEdit, WorkDoneProgressBegin,
    WorkDoneProgressCreateParams, WorkDoneProgressEnd, WorkDoneProgressOptions,
};
use std::{
    path::{Path, PathBuf},
    thread::{self, sleep, Thread},
    time::Duration,
};
use tracing::{error, info, warn};

use crate::{
    analyzer::Analyzer,
    evaluator::{self, Evaluator, LockFlakeRequest},
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

        ..Default::default()
    }
}

struct FetcherInput {
    path: PathBuf,
    source: String,
    old_lock_file: Option<String>,
}

struct FetcherOutput {
    path: PathBuf,
    lock_file: String,
}

pub fn main_loop(connection: Connection, params: serde_json::Value) -> Result<()> {
    let _params: InitializeParams = serde_json::from_value(params).unwrap();

    let (fetcher_input_send, fetcher_input_recv) = crossbeam::channel::unbounded::<FetcherInput>();
    let (fetcher_output_send, fetcher_output_recv) =
        crossbeam::channel::unbounded::<FetcherOutput>();

    let fetcher_connection_sender = connection.sender.clone();
    let fetcher_thread = thread::spawn(move || -> Result<()> {
        info!("Fetcher thread started");
        let mut evaluator = TOKIO_RUNTIME.block_on(async { Evaluator::new().await });
        let mut curr_id = 0;
        let mut curr_token = 0;
        loop {
            let input = fetcher_input_recv.recv().unwrap();
            let token = NumberOrString::Number(curr_token);
            let id = RequestId::from(curr_id);
            curr_token += 1;
            curr_id += 1;

            fetcher_connection_sender
                .send(Message::Request(Request::new(
                    id,
                    WorkDoneProgressCreate::METHOD.to_string(),
                    WorkDoneProgressCreateParams {
                        token: token.clone(),
                    },
                )))
                .unwrap();
            fetcher_connection_sender
                .send(Message::Notification(Notification::new(
                    Progress::METHOD.to_string(),
                    ProgressParams {
                        token: token.clone(),
                        value: ProgressParamsValue::WorkDone(lsp_types::WorkDoneProgress::Begin(
                            WorkDoneProgressBegin {
                                title: "Fetching flake inputs".to_string(),
                                cancellable: Some(false),
                                message: None,
                                percentage: None,
                            },
                        )),
                    },
                )))
                .unwrap();
            let response = TOKIO_RUNTIME.block_on(evaluator.lock_flake(&LockFlakeRequest {
                expression: input.source,
                old_lock_file: input.old_lock_file,
            }));

            let response = match response {
                Ok(response) => response,
                Err(err) => {
                    fetcher_connection_sender
                        .send(Message::Request(Request::new(
                            curr_id.into(),
                            ShowMessage::METHOD.to_string(),
                            ShowMessageParams {
                                typ: MessageType::ERROR,
                                message: "Failed to fetch flake inputs".to_string(),
                            },
                        )))
                        .unwrap();
                    curr_id += 1;
                    continue;
                }
            };

            fetcher_connection_sender.send(Message::Notification(Notification::new(
                Progress::METHOD.to_string(),
                ProgressParams {
                    token: token.clone(),
                    value: ProgressParamsValue::WorkDone(lsp_types::WorkDoneProgress::End(
                        WorkDoneProgressEnd { message: None },
                    )),
                },
            )))?;

            fetcher_output_send.send(FetcherOutput {
                path: input.path,
                lock_file: response.lock_file,
            })?;
        }
    });

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
                    .as_str(),
            );

            let _position = params.text_document_position_params.position;

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
                    line: 1,
                    character: 1,
                },
                end: Position {
                    line: 99999999,
                    character: 1,
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

fn handle_notification(
    analyzer: &mut Analyzer,
    not: Notification,
    sender: &Sender<FetcherInput>,
) -> Result<()> {
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
