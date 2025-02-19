#![allow(dead_code)]
#![feature(let_chains)]
#![feature(iterator_try_collect)]

mod analyzer;
mod completion;
mod evaluator;
mod file_types;
mod flakes;
mod hover;
mod lambda_arg;
mod lsp;
mod modules;
mod safe_stringification;
mod schema;
mod syntax;

use analyzer::Analyzer;
use evaluator::Evaluator;
use lsp::Backend;
use std::sync::Arc;
use tokio::sync::Mutex;
use tower_lsp::{LspService, Server};
use tracing_subscriber::{layer::SubscriberExt, util::SubscriberInitExt, EnvFilter, Layer};

#[tokio::main]
async fn main() {
    let console_layer = console_subscriber::spawn();
    tracing_subscriber::registry()
        .with(console_layer)
        .with(
            tracing_subscriber::fmt::layer()
                .with_ansi(false)
                .without_time()
                .with_writer(std::io::stderr)
                .with_filter(
                    EnvFilter::try_from_default_env()
                        .or_else(|_| EnvFilter::try_new("trace"))
                        .unwrap(),
                ),
        )
        .init();

    let stdin = tokio::io::stdin();
    let stdout = tokio::io::stdout();

    let evaluator = Arc::new(Mutex::new(Evaluator::new().await));
    let (service, socket) = LspService::new(|client| Backend {
        client,
        analyzer: Analyzer::new(evaluator.clone()),
        evaluator,
    });
    Server::new(stdin, stdout, socket).serve(service).await;
}
