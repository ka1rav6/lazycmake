use std::collections::HashMap;
use std::path::PathBuf;

#[derive(Debug, Clone)]
pub struct ThemeColors {
    pub foreground: String,
    pub background: String,
    pub panel_border_focused: String,
    pub panel_border_unfocused: String,
    pub list_inactive_foreground: String,
    pub info: String,
    pub warning: String,
    pub error: String,
}

impl Default for ThemeColors {
    fn default() -> Self {
        Self {
            foreground: "#ffffff".into(),
            background: "#1a1b26".into(),
            panel_border_focused: "#7aa2f7".into(),
            panel_border_unfocused: "#3b4261".into(),
            list_inactive_foreground: "#565f89".into(),
            info: "#7dcfff".into(),
            warning: "#e0af68".into(),
            error: "#f7768e".into(),
        }
    }
}

#[derive(Debug, Clone)]
pub struct Theme {
    pub name: String,
    pub colors: ThemeColors,
}

pub struct ThemeManager {
    themes: Vec<Theme>,
    active: usize,
}

impl ThemeManager {
    pub fn new() -> Self {
        Self {
            themes: vec![Theme {
                name: "dark".into(),
                colors: ThemeColors::default(),
            }],
            active: 0,
        }
    }

    pub fn active_colors(&self) -> &ThemeColors {
        &self.themes[self.active].colors
    }
}

#[derive(Debug, Clone)]
pub struct SettingsManager {
    pub default_generator: String,
    pub default_build_type: String,
}

impl Default for SettingsManager {
    fn default() -> Self {
        Self {
            default_generator: "Ninja".into(),
            default_build_type: "Debug".into(),
        }
    }
}

#[derive(Debug, Clone)]
pub struct KeyBinding {
    pub key: String,
    pub action: String,
}

pub struct KeymapManager {
    pub(crate) bindings: HashMap<String, Vec<KeyBinding>>,
}

impl KeymapManager {
    pub fn new() -> Self {
        let mut bindings = HashMap::new();
        bindings.insert(
            "global".into(),
            vec![
                KeyBinding { key: "q".into(), action: "quit".into() },
                KeyBinding { key: "h".into(), action: "help".into() },
            ],
        );
        bindings.insert(
            "workspace".into(),
            vec![
                KeyBinding { key: "tab".into(), action: "next_panel".into() },
                KeyBinding { key: "b".into(), action: "build".into() },
                KeyBinding { key: "r".into(), action: "run".into() },
                KeyBinding { key: "d".into(), action: "deps".into() },
            ],
        );
        Self { bindings }
    }
}
