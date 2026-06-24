# LazyCMake

> *lazygit, but for CMake* — a terminal UI for creating, configuring, building, and running CMake-based C/C++ projects without ever hand-writing a `CMakeLists.txt`.

LazyCMake gives you a fast, keyboard-driven TUI that manages your CMake project model — targets, sources, dependencies, build config — and generates human-readable CMakeLists.txt files. It never hides what it does: every generated file is written to disk, every change is tracked, and hand-edits are detected and respected.

---

## Features

- **Project model** — Targets, sources, dependencies, build config as plain data structures
- **CMake generation** — Produces modern CMake (3.21+) with `FetchContent`, `add_subdirectory`, `target_link_libraries`
- **Existing project import** — Detects and imports existing `CMakeLists.txt` files into the project model
- **Hand-edit detection** — Tracks file hashes; never silently overwrites user edits (conflict resolution dialog)
- **Dependency registry** — 10+ built-in presets (fmt, spdlog, Catch2, SDL3, GLFW, etc.) with user-customizable layers
- **EventBus** — Typed pub/sub for cross-layer communication (build progress, file changes, config reload)
- **Build & Run engines** — Async build pipeline with state machine, streaming output, and cancellation
- **Diagnostic parser** — GCC/Clang/MSVC error/warning extraction for live output highlighting
- **Configuration system** — 5-layer merged config (defaults → system → user → project → CLI)
- **Themes** — Dark, Light, Nord built-in; custom themes via JSON
- **Key rebinding** — Vim-like defaults, fully customizable per-context
- **Plugin system** — C-ABI shared library plugins with event subscription and lifecycle hooks
- **File watcher** — Polling-based filesystem change detection with extension filtering
- **TUI shell** — FTXUI-based three-panel workspace (Files | Targets | Output) with keyboard and mouse navigation
- **Project templates** — `{{placeholder}}` substitution templates for quick project bootstrap
- **Installable** — CPack packaging for DEB, RPM, tarball, NSIS

---

## Quick Start

### Prerequisites

- CMake ≥ 3.21
- C++20 compiler (GCC 11+, Clang 14+, MSVC 2022+)
- OpenSSL (dev headers for SHA256 hashing)
- Optional: FTXUI v5.0.0 (for TUI mode, fetched automatically)

### Build & Run

```bash
# Clone
git clone https://github.com/anomalyco/lazycmake.git
cd lazycmake

# Build core library + tests + examples (no TUI)
cmake -S . -B build
cmake --build build -j$(nproc)

# Run tests
./build/tests/lazycmake_core_tests

# Run examples (demonstrate each layer)
./build/examples/example01_basic_project_generation
./build/examples/example02_event_bus_usage
./build/examples/example03_config_system_demo
./build/examples/example04_build_pipeline_demo

# Build with TUI
cmake -S . -B build -DLAZYCMAKE_BUILD_TUI=ON
cmake --build build -j$(nproc)

# Launch TUI
./build/lazycmake
```

### Options

| CMake Option | Default | Description |
|---|---|---|
| `LAZYCMAKE_BUILD_TESTS` | ON | Build the Catch2 test suite |
| `LAZYCMAKE_BUILD_EXAMPLES` | ON | Build usage examples |
| `LAZYCMAKE_BUILD_TUI` | OFF | Build FTXUI-based TUI application |

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    TUI Layer (src/tui/)                   │
│  Application  MainWorkspace  Wizard  Settings  Overlays  │
│  FilePanel  TargetPanel  OutputPanel  HelpOverlay        │
├─────────────────────────────────────────────────────────┤
│              EventBus (typed pub/sub, §8)                │
├────────┬────────┬────────┬────────┬────────┬────────────┤
│ Core   │ CMake  │ Config │ Build  │ Plugin │ File       │
│ Model  │ Gen    │ System │ Engine │ Host   │ Watcher    │
│ (§6)   │ (§10)  │ (§15)  │ (§11)  │ (§16)  │ (§12)      │
└────────┴────────┴────────┴────────┴────────┴────────────┘
```

### Key Libraries

| Library | Description | Headers | Dependencies |
|---|---|---|---|
| `lazycmake_core` | Domain logic, generators, config, events, plugins | `include/lazycmake/{core,cmake_gen,config,events,build,fswatch,plugins}/` | nlohmann_json, fmt, spdlog, OpenSSL |
| `lazycmake_tui` | FTXUI-based terminal interface | `include/lazycmake/tui/` | lazycmake_core, FTXUI |
| `lazycmake` | Main executable | `src/tui/main.cpp` | lazycmake_tui |

### Boundary Rule

Only `src/tui/` and `include/lazycmake/tui/` may `#include` FTXUI headers — enforced by a configure-time grep lint. This keeps the core library compilable and testable without a terminal.

---

## Usage Guide

### Using LazyCMake in an Existing CMake Project

```bash
# In a directory with an existing CMakeLists.txt:
./build/lazycmake
```

LazyCMake detects `CMakeLists.txt` and imports the project model:
- Project name from `project()` command
- Targets from `add_executable()` / `add_library()`
- Sources, include dirs, compile features
- External dependencies from `target_link_libraries()` with `::`

After import, all CMake generation is driven from the in-memory model. Changes are synced back to disk on save.

### Creating a New Project

1. Launch LazyCMake
2. Select "New Project" from the startup screen
3. Follow the 5-step wizard:
   - **Step 1**: Project name & language (C/C++/Both)
   - **Step 2**: Target type (Executable/Static/Shared)
   - **Step 3**: C++ standard (17/20/23)
   - **Step 4**: Build system (Ninja/Unix Makefiles)
   - **Step 5**: Dependencies (check from built-in presets)
4. "Generate" writes CMakeLists.txt and project manifest

### Managing Targets & Sources

In the MainWorkspace:
- **Files panel** (left) — browse project source files, add/remove from targets
- **Targets panel** (center) — view and edit target configuration
- **Output panel** (right) — build/run output streaming

Keybindings (vim-like, rebindable):
| Key | Action | Context |
|---|---|---|
| `j`/`k` | Move down/up | Lists, menus |
| `tab`/`shift+tab` | Next/previous panel | Main workspace |
| `b` | Open build overlay | Main workspace |
| `r` | Open run overlay | Main workspace |
| `d` | Open dependency dialog | Main workspace |
| `h` | Toggle help overlay | Global |
| `q` | Quit / close | Global |
| `esc` | Close overlay / go back | Global |

### Configuration

Config files are JSON, resolved in this order (later overrides earlier):

| Layer | Location | Purpose |
|---|---|---|
| 1. Built-in defaults | Compiled into binary | Sensible defaults |
| 2. System-wide | `/etc/lazycmake/` | Admin-managed config |
| 3. User | `~/.config/lazycmake/` | Personal preferences |
| 4. Project | `<project>/.lazycmake/` | Per-project overrides |
| 5. CLI | `--setting=value` | One-off overrides |

#### Settings (`~/.config/lazycmake/settings.json`)
```json
{
    "general": {
        "language": "en",
        "restoreLastProject": true
    },
    "build": {
        "parallelJobs": 4,
        "exportCompileCommands": true
    }
}
```

#### Key Bindings (`~/.config/lazycmake/keymap.json`)
```json
{
    "global": {
        "quit": "q",
        "help": "h"
    },
    "main_workspace": {
        "build": "b",
        "run": "r"
    }
}
```

#### Themes (`~/.config/lazycmake/themes/my-theme.json`)
```json
{
    "name": "my-theme",
    "colors": {
        "background": "#1a1b26",
        "foreground": "#a9b1d6",
        "accent": "#7aa2f7"
    }
}
```

### Hand-Edit Detection & Sync

LazyCMake tracks generated files via `.lazycmake/generated.lock.json` (SHA256 hashes).

When regenerating CMake files, it checks each file:
1. **Hash matches** → safe to overwrite
2. **Hash differs** (user edited) → conflict dialog with options:
   - **View Diff** — see what changed
   - **Overwrite** — replace with generated content (backup saved as `.bak`)
   - **Keep Mine & Stop Tracking** — marks file as user-owned; LazyCMake leaves it alone

### Project Templates

Place templates in `~/.config/lazycmake/templates/`:

```
~/.config/lazycmake/templates/my-template/
├── template.json          # metadata
├── src/main.cpp.tmpl       # {{project_name}} placeholders
└── include/.gitkeep
```

`template.json`:
```json
{
    "name": "my-template",
    "description": "My custom project template",
    "defaultTargetKind": "Executable",
    "defaultSourceFiles": ["src/main.cpp"]
}
```

### Dependencies

Built-in presets: fmt, spdlog, nlohmann_json, Catch2, Boost.Asio, SDL3, GLFW, ImGui, FTXUI, EnTT

Add custom presets in `~/.config/lazycmake/dependencies.json`:
```json
{
    "my-library": {
        "description": "My in-house library",
        "gitRepo": "https://github.com/me/my-library.git",
        "gitTag": "v1.0"
    }
}
```

### Plugins

Plugins are shared libraries (`.so`/`.dylib`/`.dll`) placed in `~/.config/lazycmake/plugins/` with a manifest:

```json
{
    "name": "clang-format",
    "version": "1.0.0",
    "enabled": true,
    "library": "liblazycmake_clang_format.so"
}
```

Build the example plugin:
```bash
cd plugins/clang_format
mkdir -p build && cd build
cmake .. && make
cp liblazycmake_clang_format.so ~/.config/lazycmake/plugins/
```

---

## Project Map

```
├── CMakeLists.txt              # Root build file
├── README.md                   # This file
├── LICENSE                     # MIT License
├── docs/
│   └── DESIGN.md               # Full design document
├── include/lazycmake/
│   ├── core/                   # Project model, validation, repo, import, lock
│   ├── cmake_gen/              # CMake AST, printer, generator
│   ├── config/                 # Settings, keymap, theme, template managers
│   ├── events/                 # EventBus, PluginEventBus
│   ├── build/                  # BuildBackend, BuildManager, RunManager, DiagnosticParser
│   ├── fswatch/                # FileWatcher, FileClassifier
│   ├── plugins/                # Plugin API (IPlugin, PluginDescriptor, PluginHost)
│   └── tui/                    # Application, screens, overlays, panels
├── src/
│   ├── core/
│   ├── cmake_gen/
│   ├── config/
│   ├── events/
│   ├── build/
│   ├── fswatch/
│   ├── plugins/
│   └── tui/
│       ├── panels/             # FilePanel, TargetPanel, OutputPanel
│       └── main.cpp            # TUI entry point
├── tests/                      # Catch2 test suites (102 test cases)
├── examples/                   # 4 usage examples
├── plugins/
│   └── clang_format/           # Example plugin
├── resources/                  # Built-in templates, themes
├── cmake/
│   └── LazyCMakeConfig.cmake   # CMake config for find_package
└── build/                      # Build output
```

---

## Testing

```bash
# Run full test suite
./build/tests/lazycmake_core_tests

# Run specific test case
./build/tests/lazycmake_core_tests "EventBus"
```

Coverage: 102 test cases, 339 assertions across all layers.

---

## Packaging & Installation

```bash
# Build and package
cmake -S . -B build -DLAZYCMAKE_BUILD_TUI=ON
cmake --build build -j$(nproc)
cpack --config build/CPackConfig.cmake

# Generated packages:
#   build/LazyCMake-0.1.0-Linux.tar.gz
#   build/LazyCMake-0.1.0-Linux.deb
#   build/LazyCMake-0.1.0-Linux.rpm

# Install locally (Linux):
cmake --install build --prefix /usr/local
```

---

## Design Principles

1. **Zero manual CMake required** — every UI action maps to a generator operation
2. **Nothing is hidden** — generated files are always on disk, human-readable, re-generatable
3. **Hand-edits are detected, never silently overwritten** — hash-based conflict detection
4. **Open for extension** — plugins use the same API as built-in features
5. **Testable core** — pure domain logic with no UI dependency, 102 unit tests

---

## License

MIT — see [LICENSE](LICENSE).
