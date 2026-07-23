use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum Language {
    C,
    Cxx,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum TargetType {
    Executable,
    StaticLibrary,
    SharedLibrary,
    InterfaceLibrary,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct Target {
    pub name: String,
    pub target_type: TargetType,
    pub sources: Vec<String>,
    pub private_includes: Vec<String>,
    pub public_includes: Vec<String>,
    pub link_libraries: Vec<String>,
    pub compile_definitions: Vec<String>,
    pub cpp_standard: Option<String>,
}

impl Target {
    pub fn new(name: &str, target_type: TargetType) -> Self {
        Self {
            name: name.to_string(),
            target_type,
            sources: Vec::new(),
            private_includes: Vec::new(),
            public_includes: Vec::new(),
            link_libraries: Vec::new(),
            compile_definitions: Vec::new(),
            cpp_standard: None,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct DependencySpec {
    pub name: String,
    pub version: Option<String>,
    pub is_find_package: bool,
    pub link_name: Option<String>,
    pub enabled: bool,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct Project {
    pub name: String,
    pub version: String,
    pub language: Language,
    pub cpp_standard: String,
    pub root_dir: String,
    pub targets: Vec<Target>,
    pub dependencies: Vec<DependencySpec>,
}

impl Project {
    pub fn new(name: &str, root_dir: &str) -> Self {
        Self {
            name: name.to_string(),
            version: "0.1.0".to_string(),
            language: Language::Cxx,
            cpp_standard: "20".to_string(),
            root_dir: root_dir.to_string(),
            targets: Vec::new(),
            dependencies: Vec::new(),
        }
    }

    pub fn find_target(&self, name: &str) -> Option<&Target> {
        self.targets.iter().find(|t| t.name == name)
    }

    pub fn has_target_named(&self, name: &str) -> bool {
        self.targets.iter().any(|t| t.name == name)
    }
}
