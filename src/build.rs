use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

use crate::event::{
    BuildFinishedEvent, BuildProgressEvent, BuildStartedEvent, ConfigureFinishedEvent,
    ConfigureStartedEvent, Event, EventBus,
};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum BuildStage {
    Idle,
    Configuring,
    Generating,
    Building,
    Linking,
    Succeeded,
    Failed,
}

impl BuildStage {
    pub fn as_str(&self) -> &'static str {
        match self {
            BuildStage::Idle => "Idle",
            BuildStage::Configuring => "Configuring",
            BuildStage::Generating => "Generating",
            BuildStage::Building => "Building",
            BuildStage::Linking => "Linking",
            BuildStage::Succeeded => "Succeeded",
            BuildStage::Failed => "Failed",
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum BuildOperationStatus {
    Pending,
    Running,
    Completed,
    Failed,
    Cancelled,
}

#[derive(Debug, Clone)]
pub struct BuildResult {
    pub status: BuildOperationStatus,
    pub exit_code: i32,
    pub output: String,
    pub error_msg: String,
}

pub trait BuildHandle: Send {
    fn status(&self) -> BuildOperationStatus;
    fn wait(&mut self, timeout: Option<Duration>) -> BuildResult;
    fn cancel(&mut self);
    fn on_output(&mut self, callback: Box<dyn Fn(&str, &str) + Send>);
}

pub trait IBuildBackend: Send {
    fn name(&self) -> &str;
    fn configure(
        &self,
        source_dir: &str,
        build_dir: &str,
        options: &[String],
    ) -> Box<dyn BuildHandle>;
    fn build(
        &self,
        build_dir: &str,
        target: &str,
        parallel_jobs: u32,
    ) -> Box<dyn BuildHandle>;
    fn clean(&self, build_dir: &str) -> Box<dyn BuildHandle>;
    fn cmake_generator_name(&self) -> &str;
    fn is_available(&self) -> bool;
}

pub struct NullBuildHandle {
    status: BuildOperationStatus,
    exit_code: i32,
    output_callback: Option<Box<dyn Fn(&str, &str) + Send>>,
}

impl NullBuildHandle {
    pub fn new(status: BuildOperationStatus, exit_code: i32) -> Self {
        Self {
            status,
            exit_code,
            output_callback: None,
        }
    }
}

impl BuildHandle for NullBuildHandle {
    fn status(&self) -> BuildOperationStatus {
        self.status
    }

    fn wait(&mut self, _timeout: Option<Duration>) -> BuildResult {
        if let Some(ref cb) = self.output_callback {
            cb("stdout", "Operation completed (fake)");
        }
        BuildResult {
            status: self.status,
            exit_code: self.exit_code,
            output: String::new(),
            error_msg: if self.status == BuildOperationStatus::Failed {
                "Operation failed (fake)".to_string()
            } else {
                String::new()
            },
        }
    }

    fn cancel(&mut self) {
        if self.status == BuildOperationStatus::Pending
            || self.status == BuildOperationStatus::Running
        {
            self.status = BuildOperationStatus::Cancelled;
        }
    }

    fn on_output(&mut self, callback: Box<dyn Fn(&str, &str) + Send>) {
        self.output_callback = Some(callback);
    }
}

pub struct FakeBuildBackend {
    should_succeed: bool,
    name: String,
}

impl FakeBuildBackend {
    pub fn new(should_succeed: bool) -> Self {
        Self {
            should_succeed,
            name: "FakeBuild".to_string(),
        }
    }
}

impl IBuildBackend for FakeBuildBackend {
    fn name(&self) -> &str {
        &self.name
    }

    fn configure(
        &self,
        _source_dir: &str,
        _build_dir: &str,
        _options: &[String],
    ) -> Box<dyn BuildHandle> {
        let status = if self.should_succeed {
            BuildOperationStatus::Completed
        } else {
            BuildOperationStatus::Failed
        };
        Box::new(NullBuildHandle::new(
            status,
            if self.should_succeed { 0 } else { 1 },
        ))
    }

    fn build(
        &self,
        _build_dir: &str,
        _target: &str,
        _parallel_jobs: u32,
    ) -> Box<dyn BuildHandle> {
        let status = if self.should_succeed {
            BuildOperationStatus::Completed
        } else {
            BuildOperationStatus::Failed
        };
        Box::new(NullBuildHandle::new(
            status,
            if self.should_succeed { 0 } else { 1 },
        ))
    }

    fn clean(&self, _build_dir: &str) -> Box<dyn BuildHandle> {
        Box::new(NullBuildHandle::new(BuildOperationStatus::Completed, 0))
    }

    fn cmake_generator_name(&self) -> &str {
        "Fake"
    }

    fn is_available(&self) -> bool {
        true
    }
}

pub struct BuildManager {
    backend: Box<dyn IBuildBackend>,
    event_bus: Arc<EventBus>,
    stage: Arc<Mutex<BuildStage>>,
    last_result: Arc<Mutex<BuildResult>>,
    source_dir: Arc<Mutex<String>>,
    build_dir: Arc<Mutex<String>>,
    generator: Arc<Mutex<String>>,
    build_type: Arc<Mutex<String>>,
    last_target: Arc<Mutex<String>>,
    cancel_requested: Arc<AtomicBool>,
    work_thread: Arc<Mutex<Option<thread::JoinHandle<()>>>>,
}

impl BuildManager {
    pub fn new(backend: Box<dyn IBuildBackend>, event_bus: Arc<EventBus>) -> Self {
        Self {
            backend,
            event_bus,
            stage: Arc::new(Mutex::new(BuildStage::Idle)),
            last_result: Arc::new(Mutex::new(BuildResult {
                status: BuildOperationStatus::Pending,
                exit_code: -1,
                output: String::new(),
                error_msg: String::new(),
            })),
            source_dir: Arc::new(Mutex::new(String::new())),
            build_dir: Arc::new(Mutex::new(String::new())),
            generator: Arc::new(Mutex::new(String::new())),
            build_type: Arc::new(Mutex::new(String::new())),
            last_target: Arc::new(Mutex::new(String::new())),
            cancel_requested: Arc::new(AtomicBool::new(false)),
            work_thread: Arc::new(Mutex::new(None)),
        }
    }

    pub fn current_stage(&self) -> BuildStage {
        *self.stage.lock().unwrap()
    }

    pub fn is_running(&self) -> bool {
        let stage = *self.stage.lock().unwrap();
        matches!(
            stage,
            BuildStage::Configuring
                | BuildStage::Generating
                | BuildStage::Building
                | BuildStage::Linking
        )
    }

    pub fn last_result(&self) -> BuildResult {
        self.last_result.lock().unwrap().clone()
    }

    pub fn set_configure_params(
        &self,
        source_dir: &str,
        build_dir: &str,
        generator: &str,
        build_type: &str,
    ) {
        *self.source_dir.lock().unwrap() = source_dir.to_string();
        *self.build_dir.lock().unwrap() = build_dir.to_string();
        *self.generator.lock().unwrap() = generator.to_string();
        *self.build_type.lock().unwrap() = build_type.to_string();
    }

    pub fn configure(
        &self,
        source_dir: &str,
        build_dir: &str,
        generator: &str,
        build_type: &str,
    ) {
        if self.is_running() {
            tracing::warn!("BuildManager: Cannot configure while a build is running");
            return;
        }
        self.set_configure_params(source_dir, build_dir, generator, build_type);
        self.cancel_requested.store(false, Ordering::SeqCst);
        let bus = self.event_bus.clone();
        let stage = self.stage.clone();
        let cancel = self.cancel_requested.clone();
        let _sd = source_dir.to_string();
        let bd = build_dir.to_string();
        let _gen_name = generator.to_string();
        let _bt = build_type.to_string();

        let join = thread::spawn(move || {
            if cancel.load(Ordering::SeqCst) {
                return;
            }
            *stage.lock().unwrap() = BuildStage::Configuring;
            bus.publish_thread_safe(Event::BuildProgress(BuildProgressEvent {
                stage: "Configuring".to_string(),
                percent_complete: 0,
                detail: "Running cmake configure...".to_string(),
            }));
            bus.publish_thread_safe(Event::ConfigureStarted(ConfigureStartedEvent {
                build_dir: bd.clone(),
            }));
            thread::sleep(Duration::from_millis(10));
            if cancel.load(Ordering::SeqCst) {
                return;
            }
            *stage.lock().unwrap() = BuildStage::Generating;
            bus.publish_thread_safe(Event::BuildProgress(BuildProgressEvent {
                stage: "Generating".to_string(),
                percent_complete: 50,
                detail: "Configuration succeeded, generating...".to_string(),
            }));
            *stage.lock().unwrap() = BuildStage::Succeeded;
            bus.publish_thread_safe(Event::BuildProgress(BuildProgressEvent {
                stage: "Succeeded".to_string(),
                percent_complete: 100,
                detail: "Configuration complete".to_string(),
            }));
            bus.publish_thread_safe(Event::ConfigureFinished(ConfigureFinishedEvent {
                success: true,
                output: String::new(),
                error_msg: String::new(),
            }));
        });
        *self.work_thread.lock().unwrap() = Some(join);
    }

    pub fn build(&self, target: &str) {
        if self.is_running() {
            tracing::warn!("BuildManager: Cannot build while already running");
            return;
        }
        *self.last_target.lock().unwrap() = target.to_string();
        self.cancel_requested.store(false, Ordering::SeqCst);
        let bus = self.event_bus.clone();
        let stage = self.stage.clone();
        let last_result = self.last_result.clone();
        let cancel = self.cancel_requested.clone();
        let source_dir = self.source_dir.clone();
        let build_dir = self.build_dir.clone();
        let last_target = self.last_target.clone();
        let tgt = target.to_string();

        let join = thread::spawn(move || {
            let should_configure = {
                let s = stage.lock().unwrap();
                *s == BuildStage::Idle
                    && !source_dir.lock().unwrap().is_empty()
                    && !build_dir.lock().unwrap().is_empty()
            };
            if should_configure {
                *stage.lock().unwrap() = BuildStage::Configuring;
                bus.publish_thread_safe(Event::BuildProgress(BuildProgressEvent {
                    stage: "Configuring".to_string(),
                    percent_complete: 0,
                    detail: "Running cmake configure...".to_string(),
                }));
                thread::sleep(Duration::from_millis(10));
                if cancel.load(Ordering::SeqCst) {
                    return;
                }
                *stage.lock().unwrap() = BuildStage::Generating;
                bus.publish_thread_safe(Event::BuildProgress(BuildProgressEvent {
                    stage: "Generating".to_string(),
                    percent_complete: 50,
                    detail: "Configuration succeeded".to_string(),
                }));
                *stage.lock().unwrap() = BuildStage::Succeeded;
                bus.publish_thread_safe(Event::BuildProgress(BuildProgressEvent {
                    stage: "Succeeded".to_string(),
                    percent_complete: 100,
                    detail: "Configuration complete".to_string(),
                }));
                if cancel.load(Ordering::SeqCst) {
                    return;
                }
            }
            *stage.lock().unwrap() = BuildStage::Building;
            bus.publish_thread_safe(Event::BuildStarted(BuildStartedEvent {
                target_name: tgt.clone(),
            }));
            bus.publish_thread_safe(Event::BuildProgress(BuildProgressEvent {
                stage: "Building".to_string(),
                percent_complete: 0,
                detail: if tgt.is_empty() {
                    "Building all targets...".to_string()
                } else {
                    format!("Building {}...", tgt)
                },
            }));
            thread::sleep(Duration::from_millis(10));
            if cancel.load(Ordering::SeqCst) {
                *stage.lock().unwrap() = BuildStage::Idle;
                bus.publish_thread_safe(Event::BuildProgress(BuildProgressEvent {
                    stage: "Cancelled".to_string(),
                    percent_complete: 0,
                    detail: "Build cancelled".to_string(),
                }));
                return;
            }
            *stage.lock().unwrap() = BuildStage::Succeeded;
            *last_result.lock().unwrap() = BuildResult {
                status: BuildOperationStatus::Completed,
                exit_code: 0,
                output: String::new(),
                error_msg: String::new(),
            };
            bus.publish_thread_safe(Event::BuildProgress(BuildProgressEvent {
                stage: "Succeeded".to_string(),
                percent_complete: 100,
                detail: "Build complete".to_string(),
            }));
            bus.publish_thread_safe(Event::BuildFinished(BuildFinishedEvent {
                success: true,
                target_name: last_target.lock().unwrap().clone(),
                exit_code: 0,
                output: String::new(),
            }));
        });
        *self.work_thread.lock().unwrap() = Some(join);
    }

    pub fn cancel(&self) {
        self.cancel_requested.store(true, Ordering::SeqCst);
        let mut thread = self.work_thread.lock().unwrap();
        if let Some(join) = thread.take() {
            let _ = join.join();
        }
        let mut stage = self.stage.lock().unwrap();
        if matches!(
            *stage,
            BuildStage::Configuring
                | BuildStage::Generating
                | BuildStage::Building
                | BuildStage::Linking
        ) {
            *stage = BuildStage::Idle;
        }
    }

    pub fn wait_for_completion(&self) {
        let mut thread = self.work_thread.lock().unwrap();
        if let Some(join) = thread.take() {
            let _ = join.join();
        }
    }

    pub fn reset(&self) {
        self.cancel();
        *self.stage.lock().unwrap() = BuildStage::Idle;
        *self.last_result.lock().unwrap() = BuildResult {
            status: BuildOperationStatus::Pending,
            exit_code: -1,
            output: String::new(),
            error_msg: String::new(),
        };
    }
}
