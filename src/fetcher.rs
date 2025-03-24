use crate::{
    evaluator::{proto::HoverRequest, Evaluator, LockFlakeResponse},
    syntax::escape_string,
    TOKIO_RUNTIME,
};
use anyhow::anyhow;
use anyhow::Result;
use crossbeam::channel::{Receiver, Sender};
use lsp_server::{Message, Notification, Request};
use lsp_types::{
    notification::{Notification as _, Progress, ShowMessage},
    request::{Request as _, WorkDoneProgressCreate},
    MessageType, NumberOrString, ProgressParams, ProgressParamsValue, ShowMessageParams,
    WorkDoneProgressBegin, WorkDoneProgressCreateParams, WorkDoneProgressEnd,
};
use std::{
    collections::VecDeque,
    path::PathBuf,
    thread::{self, JoinHandle},
};
use tonic::async_trait;
use tracing::info;

#[derive(Debug)]
pub struct FetcherInput {
    pub path: PathBuf,
    pub source: String,
    pub old_lock_file: Option<String>,
}

#[derive(Debug)]
pub struct FetcherOutput {
    pub path: PathBuf,
    pub lock_file: String,
}

#[derive(Debug)]
pub struct NonBlockingFetcher {
    fetcher_input_send: Sender<FetcherInput>,
    fetcher_output_recv: Receiver<Result<FetcherOutput>>,
    fetcher_thread: JoinHandle<()>,
}

impl NonBlockingFetcher {
    pub fn new(connection_send: Sender<Message>) -> Self {
        let (fetcher_input_send, fetcher_input_recv) =
            crossbeam::channel::unbounded::<FetcherInput>();
        let (fetcher_output_send, fetcher_output_recv) =
            crossbeam::channel::unbounded::<Result<FetcherOutput>>();

        let fetcher_thread = thread::spawn(move || {
            info!("Fetcher thread started");
            NonBlockingFetcherThread::new(connection_send, fetcher_input_recv, fetcher_output_send)
                .run();
        });

        Self {
            fetcher_input_send,
            fetcher_output_recv,
            fetcher_thread,
        }
    }
}

#[derive(Debug)]
struct NonBlockingFetcherThread {
    connection_send: Sender<Message>,
    receiver: Receiver<FetcherInput>,
    sender: Sender<Result<FetcherOutput>>,
    evaluator: Evaluator,
    counter: i32,
}

impl NonBlockingFetcherThread {
    pub fn new(
        connection_send: Sender<Message>,
        receiver: Receiver<FetcherInput>,
        sender: Sender<Result<FetcherOutput>>,
    ) -> Self {
        Self {
            connection_send,
            counter: 0,
            evaluator: TOKIO_RUNTIME.block_on(Evaluator::new()),
            receiver,
            sender,
        }
    }

    fn start_progress(&mut self, title: String) -> i32 {
        self.connection_send
            .send(Message::Request(Request::new(
                self.counter.into(),
                WorkDoneProgressCreate::METHOD.to_string(),
                WorkDoneProgressCreateParams {
                    token: NumberOrString::Number(self.counter),
                },
            )))
            .unwrap();
        self.connection_send
            .send(Message::Notification(Notification::new(
                Progress::METHOD.to_string(),
                ProgressParams {
                    token: NumberOrString::Number(self.counter),
                    value: ProgressParamsValue::WorkDone(lsp_types::WorkDoneProgress::Begin(
                        WorkDoneProgressBegin {
                            title,
                            cancellable: Some(false),
                            message: None,
                            percentage: None,
                        },
                    )),
                },
            )))
            .unwrap();
        let token = self.counter;
        self.counter += 1;
        token
    }

    fn error(&self, message: String) {
        self.connection_send
            .send(Message::Notification(Notification::new(
                ShowMessage::METHOD.to_string(),
                ShowMessageParams {
                    typ: MessageType::ERROR,
                    message,
                },
            )))
            .unwrap();
    }

    fn info(&self, message: String) {
        self.connection_send
            .send(Message::Notification(Notification::new(
                ShowMessage::METHOD.to_string(),
                ShowMessageParams {
                    typ: MessageType::INFO,
                    message,
                },
            )))
            .unwrap();
    }

    fn finish_progress(&self, token: i32) {
        self.connection_send
            .send(Message::Notification(Notification::new(
                Progress::METHOD.to_string(),
                ProgressParams {
                    token: NumberOrString::Number(token),
                    value: ProgressParamsValue::WorkDone(lsp_types::WorkDoneProgress::End(
                        WorkDoneProgressEnd { message: None },
                    )),
                },
            )))
            .unwrap();
    }

    pub fn run(mut self) {
        loop {
            // if self.cancel.load(Ordering::Relaxed) {
            //     drop(self.evaluator);
            //     break;
            // }
            let input = self.receiver.recv().unwrap();
            let token = self.start_progress(format!(
                "Fetching flake inputs for {}",
                input.path.display()
            ));
            let result = TOKIO_RUNTIME.block_on(process(input, &mut self.evaluator));
            if let Err(ref err) = result {
                self.error(err.to_string());
            }

            self.sender.send(result).unwrap();

            self.finish_progress(token);
        }
    }
}

#[derive(Debug)]
pub struct BlockingFetcher {
    outputs: VecDeque<Result<FetcherOutput>>,
}

impl BlockingFetcher {
    pub fn new() -> Self {
        Self {
            outputs: VecDeque::new(),
        }
    }
}

#[async_trait]
pub trait Fetcher {
    async fn send(&mut self, input: FetcherInput, evaluator: &mut Evaluator);
    async fn try_recv(&mut self) -> Option<Result<FetcherOutput>>;
}

#[async_trait]
impl Fetcher for NonBlockingFetcher {
    async fn send(&mut self, input: FetcherInput, _evaluator: &mut Evaluator) {
        let _ = self.fetcher_input_send.send(input);
    }

    async fn try_recv(&mut self) -> Option<Result<FetcherOutput>> {
        self.fetcher_output_recv.try_recv().ok()
    }
}

#[async_trait]
impl Fetcher for BlockingFetcher {
    async fn send(&mut self, input: FetcherInput, evaluator: &mut Evaluator) {
        self.outputs.push_back(process(input, evaluator).await);
    }

    async fn try_recv(&mut self) -> Option<Result<FetcherOutput>> {
        self.outputs.pop_front()
    }
}

async fn process(input: FetcherInput, evaluator: &mut Evaluator) -> Result<FetcherOutput> {
    // let response = TOKIO_RUNTIME.block_on(self.evaluator.lock_flake(&LockFlakeRequest {
    //     expression: input.source,
    //     old_lock_file: input.old_lock_file,
    // }));

    // XXX: TODO: There is something broken when using follows, see Jackson's nix-home
    let response = match input.old_lock_file {
        Some(old_lock_file) => Ok(LockFlakeResponse {
            lock_file: old_lock_file.clone(),
        }),
        None => Err(anyhow!("No lock file")),
    }?;

    // Evaluate the flake inputs to make sure everything has been downloaded
    let _ = evaluator
        .hover(&HoverRequest {
            expression: format!(
                "__nix_analyzer_get_flake_inputs {} {{}}",
                escape_string(&response.lock_file),
            ),
            attr: None,
        })
        .await;

    Ok(FetcherOutput {
        path: input.path,
        lock_file: response.lock_file,
    })
}
