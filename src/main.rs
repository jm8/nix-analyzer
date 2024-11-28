#![allow(dead_code)]
#![feature(let_chains)]

// mod completion;
// mod evaluation;
// mod hover;
// mod safe_stringification;
mod nix_eval_server_capnp {
    include!(concat!(env!("OUT_DIR"), "/nix_eval_server_capnp.rs"));
}

use anyhow::{anyhow, Context, Result};
use bmrng::{RequestReceiver, RequestSender};
use capnp_rpc::rpc_twoparty_capnp;
use dashmap::DashMap;
use futures::AsyncReadExt as _;
use ropey::Rope;
use std::path::{Path, PathBuf};
use std::{
    net::{Ipv4Addr, SocketAddrV4},
    process::Stdio,
};
use tokio::task::{spawn_local, LocalSet};
use tokio::{
    io::{AsyncBufReadExt, BufReader},
    process::Command,
};
use tower_lsp::jsonrpc;
use tower_lsp::lsp_types::notification::PublishDiagnostics;
use tower_lsp::lsp_types::{
    CompletionItem, CompletionOptions, CompletionParams, CompletionResponse, Diagnostic,
    DiagnosticOptions, DiagnosticServerCapabilities, DidChangeTextDocumentParams,
    DidCloseTextDocumentParams, DidOpenTextDocumentParams, DidSaveTextDocumentParams,
    DocumentDiagnosticParams, DocumentDiagnosticReport, DocumentDiagnosticReportResult,
    FullDocumentDiagnosticReport, Hover, HoverContents, HoverParams, HoverProviderCapability,
    InitializeParams, InitializeResult, InitializedParams, MarkupContent, MarkupKind, MessageType,
    PublishDiagnosticsParams, RelatedFullDocumentDiagnosticReport, ServerCapabilities,
    TextDocumentSyncCapability, TextDocumentSyncKind, Url, WorkDoneProgressOptions,
};
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

struct Backend {
    client: Client,
    analyzer: Analyzer,
    tx: RequestSender<i32, i32>,
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

#[tokio::main(flavor = "current_thread")]
async fn main() {
    let stdin = tokio::io::stdin();
    let stdout = tokio::io::stdout();

    let buffer_size = 8;
    let (tx, rx) = bmrng::channel::<i32, i32>(buffer_size);

    LocalSet::new()
        .run_until(async {
            spawn_local(evaluator_thread(rx));
            let (service, socket) = LspService::new(|client| Backend {
                client,
                tx,
                analyzer: Analyzer::new(),
            });
            Server::new(stdin, stdout, socket).serve(service).await;
        })
        .await;
}

async fn evaluator_thread(mut rx: RequestReceiver<i32, i32>) {
    let p = Command::new(concat!(env!("NIX_EVAL_SERVER"), "/bin/nix-eval-server").to_owned())
        .stdout(Stdio::piped())
        .spawn()
        .context("failed to start nix-eval-server process")
        .unwrap();

    let mut reader = BufReader::new(p.stdout.unwrap());
    let mut line = String::new();
    reader.read_line(&mut line).await.unwrap();
    let port = line.trim().parse().unwrap();
    let addr = SocketAddrV4::new(Ipv4Addr::new(127, 0, 0, 1), port);
    let stream = tokio::net::TcpStream::connect(&addr).await.unwrap();
    stream.set_nodelay(true).unwrap();
    let (reader, writer) = tokio_util::compat::TokioAsyncReadCompatExt::compat(stream).split();
    let network = Box::new(capnp_rpc::twoparty::VatNetwork::new(
        reader,
        writer,
        rpc_twoparty_capnp::Side::Client,
        Default::default(),
    ));
    let mut rpc_system = capnp_rpc::RpcSystem::new(network, None);
    let evaluator: nix_eval_server_capnp::evaluator::Client =
        rpc_system.bootstrap(rpc_twoparty_capnp::Side::Server);
    tokio::task::spawn_local(rpc_system);

    while let Ok((input, responder)) = rx.recv().await {
        let mut request = evaluator.test_request();
        request.get().set_x(input as u32);
        let result = request.send().promise.await.unwrap().get().unwrap().get_y();
        responder.respond(result as i32).unwrap();
    }
}
