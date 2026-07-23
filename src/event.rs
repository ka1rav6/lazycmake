use std::collections::VecDeque;
use std::sync::Mutex;

pub type HandlerId = u64;

#[derive(Debug, Clone)]
pub struct ProjectLoadedEvent {
    pub project_name: String,
    pub project_path: String,
}

#[derive(Debug, Clone)]
pub struct ConfigureStartedEvent {
    pub build_dir: String,
}

#[derive(Debug, Clone)]
pub struct ConfigureFinishedEvent {
    pub success: bool,
    pub output: String,
    pub error_msg: String,
}

#[derive(Debug, Clone)]
pub struct BuildStartedEvent {
    pub target_name: String,
}

#[derive(Debug, Clone)]
pub struct BuildProgressEvent {
    pub stage: String,
    pub percent_complete: i32,
    pub detail: String,
}

#[derive(Debug, Clone)]
pub struct BuildFinishedEvent {
    pub success: bool,
    pub target_name: String,
    pub exit_code: i32,
    pub output: String,
}

#[derive(Debug, Clone)]
pub struct RunStartedEvent {
    pub target_name: String,
    pub executable_path: String,
}

#[derive(Debug, Clone)]
pub struct RunOutputEvent {
    pub stream: String,
    pub line: String,
}

#[derive(Debug, Clone)]
pub struct RunFinishedEvent {
    pub success: bool,
    pub exit_code: i32,
}

#[derive(Debug, Clone)]
pub struct PluginErrorEvent {
    pub plugin_name: String,
    pub error_message: String,
}

#[derive(Debug, Clone)]
pub enum Event {
    ProjectLoaded(ProjectLoadedEvent),
    ConfigureStarted(ConfigureStartedEvent),
    ConfigureFinished(ConfigureFinishedEvent),
    BuildStarted(BuildStartedEvent),
    BuildProgress(BuildProgressEvent),
    BuildFinished(BuildFinishedEvent),
    RunStarted(RunStartedEvent),
    RunOutput(RunOutputEvent),
    RunFinished(RunFinishedEvent),
    PluginError(PluginErrorEvent),
}

type HandlerFn = Box<dyn Fn(&Event) + Send + Sync + 'static>;

struct Handler {
    id: HandlerId,
    event_type: &'static str,
    func: HandlerFn,
}

pub struct EventBus {
    handlers: Mutex<Vec<Handler>>,
    next_id: Mutex<HandlerId>,
    queue: Mutex<VecDeque<Event>>,
    on_event_queued: Mutex<Option<Box<dyn Fn() + Send>>>,
}

impl EventBus {
    pub fn new() -> Self {
        Self {
            handlers: Mutex::new(Vec::new()),
            next_id: Mutex::new(1),
            queue: Mutex::new(VecDeque::new()),
            on_event_queued: Mutex::new(None),
        }
    }

    pub fn subscribe<F>(&self, event_type: &'static str, f: F) -> HandlerId
    where
        F: Fn(&Event) + Send + Sync + 'static,
    {
        let mut next = self.next_id.lock().unwrap();
        let id = *next;
        *next += 1;
        let mut handlers = self.handlers.lock().unwrap();
        handlers.push(Handler {
            id,
            event_type,
            func: Box::new(f),
        });
        id
    }

    pub fn unsubscribe(&self, id: HandlerId) {
        let mut handlers = self.handlers.lock().unwrap();
        handlers.retain(|h| h.id != id);
    }

    pub fn publish(&self, event: &Event) {
        let type_name = event.type_name();
        let handlers = self.handlers.lock().unwrap();
        for handler in handlers.iter() {
            if handler.event_type == type_name {
                (handler.func)(event);
            }
        }
    }

    pub fn publish_thread_safe(&self, event: Event) {
        {
            let mut queue = self.queue.lock().unwrap();
            queue.push_back(event);
        }
        if let Ok(cb_lock) = self.on_event_queued.lock() {
            if let Some(ref cb) = *cb_lock {
                cb();
            }
        }
    }

    pub fn drain_queue(&self) {
        let mut local = {
            let mut queue = self.queue.lock().unwrap();
            std::mem::take(&mut *queue)
        };
        while let Some(event) = local.pop_front() {
            self.publish(&event);
        }
    }

    pub fn pending_count(&self) -> usize {
        self.queue.lock().unwrap().len()
    }

    pub fn set_on_event_queued<F>(&self, f: F)
    where
        F: Fn() + Send + 'static,
    {
        let mut cb = self.on_event_queued.lock().unwrap();
        *cb = Some(Box::new(f));
    }

    pub fn clear(&self) {
        self.handlers.lock().unwrap().clear();
        self.queue.lock().unwrap().clear();
    }
}

impl Event {
    pub fn type_name(&self) -> &'static str {
        match self {
            Event::ProjectLoaded(_) => "ProjectLoaded",
            Event::ConfigureStarted(_) => "ConfigureStarted",
            Event::ConfigureFinished(_) => "ConfigureFinished",
            Event::BuildStarted(_) => "BuildStarted",
            Event::BuildProgress(_) => "BuildProgress",
            Event::BuildFinished(_) => "BuildFinished",
            Event::RunStarted(_) => "RunStarted",
            Event::RunOutput(_) => "RunOutput",
            Event::RunFinished(_) => "RunFinished",
            Event::PluginError(_) => "PluginError",
        }
    }
}
