use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

use crate::event::{Event, EventBus, RunFinishedEvent, RunOutputEvent, RunStartedEvent};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RunState {
    Idle,
    Launching,
    Running,
    Finished,
    Killed,
}

impl RunState {
    pub fn as_str(&self) -> &'static str {
        match self {
            RunState::Idle => "Idle",
            RunState::Launching => "Launching",
            RunState::Running => "Running",
            RunState::Finished => "Finished",
            RunState::Killed => "Killed",
        }
    }
}

#[derive(Debug, Clone)]
pub struct RunResult {
    pub success: bool,
    pub exit_code: i32,
    pub output: String,
    pub error_msg: String,
}

pub trait IRunBackend: Send {
    fn launch(
        &mut self,
        executable_path: &str,
        args: &[String],
        working_dir: &str,
        env_vars: &[String],
        on_output: Box<dyn Fn(&str, &str) + Send>,
    ) -> bool;
    fn wait(&mut self, timeout_ms: i32) -> RunResult;
    fn kill(&mut self);
    fn is_running(&self) -> bool;
    fn pid(&self) -> i32;
}

pub struct FakeRunBackend {
    should_succeed: bool,
    exit_code: i32,
    launched: bool,
    killed: bool,
    output_callback: Option<Box<dyn Fn(&str, &str) + Send>>,
}

impl FakeRunBackend {
    pub fn new(should_succeed: bool, exit_code: i32) -> Self {
        Self {
            should_succeed,
            exit_code,
            launched: false,
            killed: false,
            output_callback: None,
        }
    }
}

impl IRunBackend for FakeRunBackend {
    fn launch(
        &mut self,
        executable_path: &str,
        args: &[String],
        _working_dir: &str,
        _env_vars: &[String],
        on_output: Box<dyn Fn(&str, &str) + Send>,
    ) -> bool {
        self.launched = true;
        self.killed = false;
        self.output_callback = Some(on_output);
        if let Some(ref cb) = self.output_callback {
            cb("stdout", &format!("Launching {}...", executable_path));
            cb(
                "stdout",
                &format!("Running with {} argument(s)", args.len()),
            );
        }
        true
    }

    fn wait(&mut self, _timeout_ms: i32) -> RunResult {
        if !self.launched {
            return RunResult {
                success: false,
                exit_code: -1,
                output: String::new(),
                error_msg: "Process was never launched".to_string(),
            };
        }
        if self.killed {
            return RunResult {
                success: false,
                exit_code: -1,
                output: "Process was killed".to_string(),
                error_msg: "Killed".to_string(),
            };
        }
        thread::sleep(Duration::from_millis(10));
        if let Some(ref cb) = self.output_callback {
            if self.should_succeed {
                cb("stdout", "Process completed successfully");
            } else {
                cb(
                    "stderr",
                    &format!("Process failed with exit code {}", self.exit_code),
                );
            }
        }
        RunResult {
            success: self.should_succeed,
            exit_code: self.exit_code,
            output: String::new(),
            error_msg: if self.should_succeed {
                String::new()
            } else {
                "Simulated failure".to_string()
            },
        }
    }

    fn kill(&mut self) {
        self.killed = true;
        if let Some(ref cb) = self.output_callback {
            cb("stdout", "Process killed by user");
        }
    }

    fn is_running(&self) -> bool {
        self.launched && !self.killed
    }

    fn pid(&self) -> i32 {
        if self.launched {
            12345
        } else {
            -1
        }
    }
}

pub struct RunManager {
    event_bus: Arc<EventBus>,
    backend: Arc<Mutex<Box<dyn IRunBackend>>>,
    state: Arc<Mutex<RunState>>,
    last_result: Arc<Mutex<RunResult>>,
    cancel_requested: Arc<AtomicBool>,
    work_thread: Arc<Mutex<Option<thread::JoinHandle<()>>>>,
}

impl RunManager {
    pub fn new(event_bus: Arc<EventBus>) -> Self {
        Self {
            event_bus,
            backend: Arc::new(Mutex::new(Box::new(FakeRunBackend::new(true, 0)))),
            state: Arc::new(Mutex::new(RunState::Idle)),
            last_result: Arc::new(Mutex::new(RunResult {
                success: false,
                exit_code: -1,
                output: String::new(),
                error_msg: String::new(),
            })),
            cancel_requested: Arc::new(AtomicBool::new(false)),
            work_thread: Arc::new(Mutex::new(None)),
        }
    }

    pub fn set_backend(&self, backend: Box<dyn IRunBackend>) {
        self.kill();
        *self.backend.lock().unwrap() = backend;
    }

    pub fn current_state(&self) -> RunState {
        *self.state.lock().unwrap()
    }

    pub fn is_running(&self) -> bool {
        let s = *self.state.lock().unwrap();
        s == RunState::Launching || s == RunState::Running
    }

    pub fn last_result(&self) -> RunResult {
        self.last_result.lock().unwrap().clone()
    }

    pub fn run(
        &self,
        executable_path: &str,
        args: &[String],
        working_dir: &str,
        env_vars: &[String],
    ) {
        if self.is_running() {
            tracing::warn!("RunManager: A process is already running");
            return;
        }
        self.cancel_requested.store(false, Ordering::SeqCst);
        let bus = self.event_bus.clone();
        let backend = self.backend.clone();
        let state = self.state.clone();
        let last_result = self.last_result.clone();
        let cancel = self.cancel_requested.clone();
        let exe = executable_path.to_string();
        let args = args.to_vec();
        let wd = working_dir.to_string();
        let env = env_vars.to_vec();

        let join = thread::spawn(move || {
            if cancel.load(Ordering::SeqCst) {
                return;
            }
            *state.lock().unwrap() = RunState::Launching;
            bus.publish_thread_safe(Event::RunStarted(RunStartedEvent {
                target_name: exe.clone(),
                executable_path: exe.clone(),
            }));
            let bus_clone = Arc::clone(&bus);
            let mut bk = backend.lock().unwrap();
            let launched = bk.launch(
                &exe,
                &args,
                &wd,
                &env,
                Box::new(move |stream: &str, line: &str| {
                    bus_clone.publish_thread_safe(Event::RunOutput(RunOutputEvent {
                        stream: stream.to_string(),
                        line: line.to_string(),
                    }));
                }),
            );
            if !launched {
                *state.lock().unwrap() = RunState::Finished;
                *last_result.lock().unwrap() = RunResult {
                    success: false,
                    exit_code: -1,
                    output: String::new(),
                    error_msg: format!("Failed to launch {}", exe),
                };
                bus.publish_thread_safe(Event::RunFinished(RunFinishedEvent {
                    success: false,
                    exit_code: -1,
                }));
                return;
            }
            if cancel.load(Ordering::SeqCst) {
                return;
            }
            *state.lock().unwrap() = RunState::Running;
            let result = bk.wait(0);
            *last_result.lock().unwrap() = result.clone();
            *state.lock().unwrap() = RunState::Finished;
            bus.publish_thread_safe(Event::RunFinished(RunFinishedEvent {
                success: result.success,
                exit_code: result.exit_code,
            }));
        });
        *self.work_thread.lock().unwrap() = Some(join);
    }

    pub fn kill(&self) {
        self.cancel_requested.store(true, Ordering::SeqCst);
        {
            let mut bk = self.backend.lock().unwrap();
            bk.kill();
        }
        let mut thread = self.work_thread.lock().unwrap();
        if let Some(join) = thread.take() {
            let _ = join.join();
        }
        *self.state.lock().unwrap() = RunState::Killed;
    }

    pub fn wait_for_completion(&self) {
        let mut thread = self.work_thread.lock().unwrap();
        if let Some(join) = thread.take() {
            let _ = join.join();
        }
    }
}
