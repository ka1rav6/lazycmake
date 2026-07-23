use std::sync::atomic::Ordering;
use std::sync::Arc;
use std::time::Duration;

use crossterm::event::{self, Event, KeyCode, KeyEventKind};
use ratatui::layout::{Constraint, Direction, Layout, Rect};
use ratatui::style::{Color, Style};
use ratatui::text::{Line, Span};
use ratatui::widgets::{Block, Borders, List, ListItem, Paragraph, Wrap};
use ratatui::Frame;

use crate::build::{BuildManager, BuildStage};
use crate::config::{KeymapManager, SettingsManager, ThemeManager};
use crate::event::{BuildFinishedEvent, Event as AppEvent, EventBus, RunFinishedEvent, RunOutputEvent};
use crate::run::RunManager;
use crate::tui::workspace;

#[derive(Clone, Copy, PartialEq, Eq)]
pub enum Screen {
    Startup,
    Workspace,
}

pub struct TuiApplication {
    pub event_bus: Arc<EventBus>,
    pub build_manager: BuildManager,
    pub run_manager: RunManager,
    pub theme_manager: ThemeManager,
    pub keymap_manager: KeymapManager,
    pub settings: SettingsManager,
    pub screen: Screen,
    pub should_quit: bool,

    // Workspace state
    pub output_lines: Vec<String>,
    pub build_overlay_visible: bool,
    pub run_overlay_visible: bool,
    pub help_overlay_visible: bool,
    pub build_log_lines: Vec<String>,
    pub run_log_lines: Vec<String>,
    pub is_building: bool,
    pub is_running: bool,
    pub active_panel: usize, // 0=files, 1=targets, 2=output

    // Startup menu
    pub startup_selected: usize,
    pub startup_items: Vec<String>,
}

impl TuiApplication {
    pub fn new() -> Self {
        let event_bus = Arc::new(EventBus::new());
        let build_manager =
            BuildManager::new(Box::new(crate::build::FakeBuildBackend::new(true)), event_bus.clone());
        let run_manager = RunManager::new(event_bus.clone());

        let mut app = Self {
            event_bus: event_bus.clone(),
            build_manager,
            run_manager,
            theme_manager: ThemeManager::new(),
            keymap_manager: KeymapManager::new(),
            settings: SettingsManager::default(),
            screen: Screen::Startup,
            should_quit: false,
            output_lines: vec![
                "Welcome to LazyCMake!".into(),
                "".into(),
                "This is the Output panel.".into(),
                "Build and run output will appear here.".into(),
            ],
            build_overlay_visible: false,
            run_overlay_visible: false,
            help_overlay_visible: false,
            build_log_lines: Vec::new(),
            run_log_lines: Vec::new(),
            is_building: false,
            is_running: false,
            active_panel: 0,
            startup_selected: 0,
            startup_items: vec![
                "New Project".into(),
                "Open Project".into(),
                "Settings".into(),
                "Quit".into(),
            ],
        };

        app.event_bus.set_on_event_queued(|| {});
        app
    }

    pub fn setup_subscriptions(&mut self) {
        let lines = self.output_lines.clone();
        let output = Arc::new(std::sync::Mutex::new(lines));

        {
            let out = output.clone();
            let ol = self.output_lines.clone();
            self.event_bus.subscribe("RunOutput", {
                move |e: &AppEvent| {
                    if let AppEvent::RunOutput(ev) = e {
                        let mut o = out.lock().unwrap();
                        o.push(format!("[{}] {}", ev.stream, ev.line));
                    }
                }
            });
        }

        {
            let out = output.clone();
            self.event_bus.subscribe("BuildFinished", {
                move |e: &AppEvent| {
                    if let AppEvent::BuildFinished(ev) = e {
                        let mut o = out.lock().unwrap();
                        if ev.success {
                            o.push(format!(
                                "[build] Finished target '{}' (exit {})",
                                ev.target_name, ev.exit_code
                            ));
                        } else {
                            o.push(format!(
                                "[build] FAILED target '{}' (exit {})",
                                ev.target_name, ev.exit_code
                            ));
                        }
                    }
                }
            });
        }

        {
            let out = output.clone();
            self.event_bus.subscribe("RunFinished", {
                move |e: &AppEvent| {
                    if let AppEvent::RunFinished(ev) = e {
                        let mut o = out.lock().unwrap();
                        if ev.success {
                            o.push(format!("[run] Process exited with code {}", ev.exit_code));
                        } else {
                            o.push(format!("[run] Process failed with code {}", ev.exit_code));
                        }
                    }
                }
            });
        }
    }

    pub fn run(&mut self) -> color_eyre::Result<()> {
        color_eyre::install()?;
        let mut terminal = ratatui::init();
        terminal.clear()?;

        self.setup_subscriptions();

        while !self.should_quit {
            self.event_bus.drain_queue();
            terminal.draw(|f| self.render(f))?;
            self.handle_events()?;
        }

        ratatui::restore();
        Ok(())
    }

    fn render(&mut self, frame: &mut Frame) {
        match self.screen {
            Screen::Startup => self.render_startup(frame),
            Screen::Workspace => self.render_workspace(frame),
        }
    }

    fn render_startup(&self, frame: &mut Frame) {
        let colors = self.theme_manager.active_colors();
        let items: Vec<ListItem> = self
            .startup_items
            .iter()
            .enumerate()
            .map(|(i, item)| {
                let style = if i == self.startup_selected {
                    Style::default().fg(Color::Rgb(0x7a, 0xa2, 0xf7))
                } else {
                    Style::default().fg(Color::Rgb(0x56, 0x5f, 0x89))
                };
                ListItem::new(Line::from(Span::styled(item, style)))
            })
            .collect();

        let list = List::new(items)
            .block(
                Block::bordered()
                    .title(" LazyCMake ")
                    .title_alignment(ratatui::layout::Alignment::Center),
            )
            .highlight_style(Style::default().fg(Color::Rgb(0x7a, 0xa2, 0xf7)));

        let area = frame.area();
        let vert = Layout::default()
            .direction(Direction::Vertical)
            .constraints([
                Constraint::Length(2),
                Constraint::Min(0),
                Constraint::Length(1),
            ])
            .split(area);

        let title = Paragraph::new("lazygit, but for CMake")
            .style(Style::default().fg(Color::Rgb(0x56, 0x5f, 0x89)))
            .alignment(ratatui::layout::Alignment::Center);
        frame.render_widget(title, vert[0]);

        let list_area = Layout::default()
            .direction(Direction::Horizontal)
            .constraints([Constraint::Percentage(30), Constraint::Percentage(40), Constraint::Percentage(30)])
            .split(vert[1]);
        frame.render_widget(list, list_area[1]);

        let footer = Paragraph::new(" j/k  navigate  enter  select  q  quit  h  help ")
            .style(Style::default().fg(Color::Rgb(0x56, 0x5f, 0x89)))
            .alignment(ratatui::layout::Alignment::Center);
        frame.render_widget(footer, vert[2]);
    }

    fn render_workspace(&mut self, frame: &mut Frame) {
        let colors = self.theme_manager.active_colors();
        let area = frame.area();

        // Update output lines from shared buffer
        {
            // Lines are appended via event handlers, already in self.output_lines
        }

        // Three panels horizontally
        let panel_layout = Layout::default()
            .direction(Direction::Horizontal)
            .constraints([Constraint::Percentage(33), Constraint::Percentage(33), Constraint::Percentage(34)])
            .split(area);

        // File panel
        workspace::render_file_panel(frame, panel_layout[0], self.active_panel == 0, &colors);
        // Target panel
        workspace::render_target_panel(frame, panel_layout[1], self.active_panel == 1, &colors);
        // Output panel
        workspace::render_output_panel(frame, panel_layout[2], self.active_panel == 2, &colors, &self.output_lines);

        // Overlays on top
        let full = area;
        if self.build_overlay_visible {
            workspace::render_build_overlay(frame, full, &self.build_log_lines, self.build_manager.current_stage(), self.is_building, &colors);
        }
        if self.run_overlay_visible {
            workspace::render_run_overlay(frame, full, &self.run_log_lines, self.is_running, &colors);
        }
        if self.help_overlay_visible {
            workspace::render_help_overlay(frame, full, &colors);
        }
    }

    fn handle_events(&mut self) -> color_eyre::Result<()> {
        if !event::poll(Duration::from_millis(50))? {
            return Ok(());
        }
        let ev = event::read()?;
        match self.screen {
            Screen::Startup => self.handle_startup_event(ev),
            Screen::Workspace => self.handle_workspace_event(ev),
        }
        Ok(())
    }

    fn handle_startup_event(&mut self, ev: Event) {
        if let Event::Key(key) = ev {
            if key.kind != KeyEventKind::Press {
                return;
            }
            match key.code {
                KeyCode::Char('q') => self.should_quit = true,
                KeyCode::Char('h') => self.help_overlay_visible = !self.help_overlay_visible,
                KeyCode::Char('j') | KeyCode::Down => {
                    self.startup_selected = (self.startup_selected + 1).min(self.startup_items.len() - 1);
                }
                KeyCode::Char('k') | KeyCode::Up => {
                    self.startup_selected = self.startup_selected.saturating_sub(1);
                }
                KeyCode::Enter => {
                    match self.startup_selected {
                        0 => { /* New Project - just go to workspace */ }
                        1 => { /* Open Project - go to workspace */ }
                        2 => { /* Settings */ }
                        3 => self.should_quit = true,
                        _ => {}
                    }
                    self.screen = Screen::Workspace;
                    self.build_manager.set_configure_params("./", "./build", "Ninja", "Debug");
                }
                KeyCode::Esc => {
                    self.help_overlay_visible = false;
                }
                _ => {}
            }
        }
    }

    fn handle_workspace_event(&mut self, ev: Event) {
        if let Event::Key(key) = ev {
            if key.kind != KeyEventKind::Press {
                return;
            }
            match key.code {
                KeyCode::Char('q') => {
                    self.screen = Screen::Startup;
                    self.build_overlay_visible = false;
                    self.run_overlay_visible = false;
                    self.help_overlay_visible = false;
                }
                KeyCode::Tab => {
                    self.active_panel = (self.active_panel + 1) % 3;
                }
                KeyCode::BackTab => {
                    self.active_panel = (self.active_panel + 2) % 3;
                }
                KeyCode::Char('b') => {
                    self.build_overlay_visible = !self.build_overlay_visible;
                    if self.build_overlay_visible && !self.is_building {
                        self.build_log_lines.clear();
                        let stage = self.build_manager.current_stage();
                        if stage == BuildStage::Idle {
                            self.build_manager.set_configure_params("./", "./build", "Ninja", "Debug");
                        }
                        self.is_building = true;
                        self.build_manager.build("my_app");
                    }
                }
                KeyCode::Char('c') => {
                    if self.is_building {
                        self.build_manager.cancel();
                        self.is_building = false;
                        self.build_log_lines.push("[cancelled] Build cancelled by user".into());
                    }
                }
                KeyCode::Char('r') => {
                    self.run_overlay_visible = !self.run_overlay_visible;
                    if self.run_overlay_visible && !self.is_running {
                        self.run_log_lines.clear();
                        self.is_running = true;
                        self.run_manager.run("./build/my_app", &[], "./", &[]);
                    }
                }
                KeyCode::Char('k') => {
                    if self.is_running {
                        self.run_manager.kill();
                        self.is_running = false;
                        self.run_log_lines.push("[killed] Process killed by user".into());
                    }
                }
                KeyCode::Char('h') => {
                    self.help_overlay_visible = !self.help_overlay_visible;
                }
                KeyCode::Esc => {
                    if self.build_overlay_visible {
                        self.build_overlay_visible = false;
                    } else if self.run_overlay_visible {
                        self.run_overlay_visible = false;
                    } else if self.help_overlay_visible {
                        self.help_overlay_visible = false;
                    }
                }
                _ => {}
            }
        }
    }
}
