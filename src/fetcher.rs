use crossbeam::channel::{Receiver, Sender};
use lsp_server::{Message, Notification, Request};
use lsp_types::{
    notification::{Notification as _, Progress, ShowMessage},
    request::{Request as _, WorkDoneProgressCreate},
    MessageType, NumberOrString, ProgressParams, ProgressParamsValue, ShowMessageParams,
    WorkDoneProgressBegin, WorkDoneProgressCreateParams, WorkDoneProgressEnd,
};
use std::{path::PathBuf, thread::sleep, time::Duration};

use crate::{
    evaluator::{Evaluator, LockFlakeRequest},
    TOKIO_RUNTIME,
};

pub struct FetcherInput {
    pub path: PathBuf,
    pub source: String,
    pub old_lock_file: Option<String>,
}

pub struct FetcherOutput {
    pub path: PathBuf,
    pub lock_file: String,
}

pub struct Fetcher {
    pub sender: Sender<FetcherOutput>,
    pub receiver: Receiver<FetcherInput>,
    pub connection_send: Sender<Message>,
    evaluator: Evaluator,
    counter: i32,
}

impl Fetcher {
    pub fn new(
        sender: Sender<FetcherOutput>,
        receiver: Receiver<FetcherInput>,
        connection_send: Sender<Message>,
    ) -> Self {
        Self {
            sender,
            receiver,
            connection_send,
            counter: 0,
            evaluator: TOKIO_RUNTIME.block_on(Evaluator::new()),
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

    pub fn run(&mut self) {
        loop {
            let input = self.receiver.recv().unwrap();

            let token = self.start_progress(format!(
                "Fetching flake inputs for {}",
                input.path.display()
            ));

            sleep(Duration::from_secs(5));

            let response = TOKIO_RUNTIME.block_on(self.evaluator.lock_flake(&LockFlakeRequest {
                expression: input.source,
                old_lock_file: input.old_lock_file,
            }));

            match response {
                Ok(response) => {
                    self.info(response.lock_file.to_string());
                    self.sender
                        .send(FetcherOutput {
                            path: input.path,
                            lock_file: response.lock_file,
                        })
                        .unwrap();
                }
                Err(_) => {
                    self.error(format!(
                        "Failed to fetch flake inputs for {}",
                        input.path.display()
                    ));
                }
            };

            self.finish_progress(token);
        }
    }
}
