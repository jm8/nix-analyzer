mod nix_eval_server_capnp {
    include!(concat!(env!("OUT_DIR"), "/nix_eval_server_capnp.rs"));
}

use anyhow::Context;
use bmrng::{RequestReceiver, RequestSender};
use capnp_rpc::rpc_twoparty_capnp;
use futures::AsyncReadExt as _;
use std::{
    net::{Ipv4Addr, SocketAddrV4},
    process::Stdio,
};
use tokio::{
    io::{AsyncBufReadExt, BufReader},
    process::Command,
};
use tower_lsp::lsp_types::request;

pub enum EvaluatorRequest {
    GetAttributesRequest(String),
}

pub enum EvaluatorResponse {
    GetAttributesResponse(Vec<String>),
}

pub struct Evaluator {
    pub tx: RequestSender<EvaluatorRequest, EvaluatorResponse>,
}

pub async fn evaluator_thread(mut rx: RequestReceiver<EvaluatorRequest, EvaluatorResponse>) {
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
        match input {
            EvaluatorRequest::GetAttributesRequest(_) => todo!(),
        }
    }
}
