#![allow(dead_code)]
#![feature(let_chains)]
#![feature(iterator_try_collect)]

mod analyzer;
mod completion;
mod evaluator;
mod fetcher;
mod file_types;
mod flakes;
mod hover;
mod lambda_arg;
mod lsp;
mod modules;
mod safe_stringification;
mod schema;
mod syntax;
mod walk_attrs;

use anyhow::Result;
use lazy_static::lazy_static;
use lsp::{capabilities, main_loop};
use lsp_server::Connection;
use tokio::runtime::Runtime;
use tracing_subscriber::{layer::SubscriberExt, util::SubscriberInitExt, EnvFilter, Layer};

lazy_static! {
    static ref TOKIO_RUNTIME: Runtime = tokio::runtime::Builder::new_multi_thread()
        .thread_name("tokio")
        // .max_blocking_threads(MAX_BLOCKING_THREADS)
        .enable_all()
        .build()
        .unwrap();
}

fn main() -> Result<()> {
    // let console_layer = console_subscriber::spawn();
    tracing_subscriber::registry()
        // .with(console_layer)
        .with(
            tracing_subscriber::fmt::layer()
                .with_ansi(false)
                .without_time()
                .with_writer(std::io::stderr)
                .with_filter(
                    EnvFilter::try_from_default_env()
                        .or_else(|_| {
                            EnvFilter::try_new("nix_analyzer_new=trace,lsp_server::msg=debug,info")
                        })
                        .unwrap(),
                ),
        )
        .init();

    let (connection, io_threads) = Connection::stdio();
    let server_capabilities = serde_json::to_value(capabilities()).unwrap();
    let initialization_params = match connection.initialize(server_capabilities) {
        Ok(it) => it,
        Err(e) => {
            if e.channel_is_disconnected() {
                io_threads.join()?;
            }
            return Err(e.into());
        }
    };

    main_loop(connection, initialization_params)?;

    Ok(())
}
