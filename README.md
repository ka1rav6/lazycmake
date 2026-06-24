<div align="center">

# ⚡ LazyCMake

**lazygit, but for CMake** — a terminal UI for creating, configuring, building, and running CMake C/C++ projects without ever hand-writing a `CMakeLists.txt`.

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)](https://en.cppreference.com/w/cpp/20)
[![CMake ≥ 3.21](https://img.shields.io/badge/CMake-%E2%89%A5%203.21-brightgreen)](https://cmake.org)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow)](LICENSE)

</div>

---

## 🚀 Quick Start

```bash
# Build (TUI enabled)
cmake -S . -B build -DLAZYCMAKE_BUILD_TUI=ON
cmake --build build -j$(nproc)

# Install to ~/bin for quick access
cp build/lazycmake ~/bin/

# Launch
lazycmake
```

That's it. The startup screen appears — press `j`/`k` to navigate, `Enter` to select "New Project", follow the 5-step wizard, and LazyCMake generates a complete `CMakeLists.txt` for you.

---

## 📖 Table of Contents

- [Features](#-features)
- [Installation](#-installation)
- [First Run](#-first-run)
- [TUI Walkthrough](#-tui-walkthrough)
- [Configuration](#-configuration)
- [Dependencies](#-dependencies)
- [Plugins](#-plugins)
- [Hand-Edit Sync](#-hand-edit-sync)
- [Project Templates](#-project-templates)
- [Architecture](#-architecture)
- [Testing](#-testing)
- [Packaging](#-packaging)
- [Project Map](#-project-map)
- [Keyboard Reference](#-keyboard-reference)
- [Design Principles](#-design-principles)
- [License](#-license)

---

## ✨ Features

| | Feature | Description |
|---|---|---|
| 🖥️ | **TUI Shell** | FTXUI-based three-panel workspace (Files \| Targets \| Output) with full keyboard and mouse navigation |
| 📐 | **CMake Generation** | Produces modern CMake (3.21+) with `FetchContent`, `add_subdirectory`, `target_link_libraries` |
| 📂 | **Existing Project Import** | Reads existing `CMakeLists.txt` and reconstructs the project model |
| 🔍 | **Hand-Edit Detection** | SHA256 hash tracking; never silently overwrites user edits (conflict dialog) |
| 📦 | **Dependency Registry** | 10+ built-in presets (fmt, spdlog, Catch2, SDL3, GLFW, ImGui, FTXUI, EnTT, Boost.Asio, nlohmann_json) + user presets |
| 🔌 | **Plugin System** | C-ABI shared library plugins with event subscription, lifecycle hooks |
| ⚙️ | **Configuration** | 5-layer merged config (defaults → system → user → project → CLI) with JSON files |
| 🎨 | **Themes** | Dark, Light, Nord built-in; custom themes via JSON |
| ⌨️ | **Key Rebinding** | Vim-like defaults, fully customizable per-context |
| 🏗️ | **Build Engine** | Async CMake build pipeline with streaming output and cancellation |
| ▶️ | **Run Engine** | Launch built executables with live output streaming |
| 👁️ | **File Watcher** | Polling-based filesystem change detection with extension filtering |
| 🩺 | **Diagnostic Parser** | GCC/Clang/MSVC error/warning extraction for live output |
| 📋 | **Project Templates** | `{{placeholder}}` substitution templates for quick bootstrap |
| 📦 | **Installable** | CPack packaging for DEB, RPM, tarball, NSIS |

---

## 🛠 Installation

### Prerequisites

| Dependency | Minimum | Notes |
|---|---|---|
| CMake | 3.21 | Build system |
| C++ Compiler | C++20 | GCC 11+, Clang 14+, MSVC 2022+ |
| OpenSSL | Any | Dev headers for SHA256 (optional — falls back to `std::hash`) |

### Build Options

| CMake Option | Default | Description |
|---|---|---|
| `LAZYCMAKE_BUILD_TUI` | OFF | Build the FTXUI-based TUI application |
| `LAZYCMAKE_BUILD_TESTS` | ON | Build the Catch2 test suite (102 test cases) |
| `LAZYCMAKE_BUILD_EXAMPLES` | ON | Build usage examples |

### Build from Source

```bash
# 1. Clone
git clone https://github.com/anomalyco/lazycmake.git
cd lazycmake

# 2. Core library + tests + examples (no TUI)
cmake -S . -B build
cmake --build build -j$(nproc)
./build/tests/lazycmake_core_tests

# 3. Enable TUI (fetches FTXUI automatically)
cmake -S . -B build -DLAZYCMAKE_BUILD_TUI=ON
cmake --build build -j$(nproc)

# 4. Install to PATH
cp build/lazycmake ~/bin/
```

### Verify Installation

```bash
lazycmake --help     # Should show startup screen
```

---

## 🎬 First Run

When you launch `lazycmake`, you'll see the **Startup Screen**:

```
╔══════════════════════════════════════════════════════════╗
║                     LazyCMake                            ║
║              lazygit, but for CMake                      ║
║                                                          ║
║  ───────────────────────────────────────────────────────  ║
║                                                          ║
║               New Project       ← select with j/k        ║
║               Open Project      ← Enter to confirm       ║
║               Recent Projects                            ║
║               Templates                                  ║
║               Settings                                   ║
║               Quit                                       ║
║                                                          ║
║  ───────────────────────────────────────────────────────  ║
║  Navigate: j/k   Select: enter   Quit: q   Help: h      ║
╚══════════════════════════════════════════════════════════╝
```

### Creating Your First Project

1. Press `Enter` on **New Project**
2. Follow the 5-step wizard:

| Step | Field | Your Input |
|---|---|---|
| 1 | Project Name | `my_game` |
| 1 | Language | `C++` (use toggle) |
| 1 | Directory | `.` (current dir) |
| 2 | Target Type | `Executable` |
| 3 | C++ Standard | `C++20` |
| 4 | Build System | `Ninja` |
| 5 | Dependencies | `[x] fmt`, `[x] spdlog` |

3. Press **Generate** — LazyCMake writes:
   - `CMakeLists.txt` with proper `project()`, `add_executable()`, `FetchContent`, `target_link_libraries()`
   - `.lazycmake/project.json` — your project model
   - `src/main.cpp` — a minimal stub

4. You're dropped into the **Main Workspace**:

```
┌───────────── Files ─────────────┬───────── Targets ──────────┬─────────── Output ───────────┐
│  src/main.cpp                   │  my_game (Executable)      │  Welcome to LazyCMake!        │
│  CMakeLists.txt                 │                            │                                │
│  include/                       │                            │  [build] Configuring...        │
└─────────────────────────────────┴────────────────────────────┴────────────────────────────────┘
```

5. Press `b` to build, `r` to run, `d` to manage dependencies.

---

## 🧭 TUI Walkthrough

### Main Workspace

The main workspace has three panels:

**Files panel** (left side)
- Lists all source files in the project directory
- Navigate with `j`/`k`
- Hidden files (`.xxx`) and `build/` are excluded

**Targets panel** (center)
- Shows all project targets with their types (Executable, StaticLibrary, SharedLibrary)
- Navigate with `j`/`k`

**Output panel** (right side)
- Streaming output from build/run operations
- Auto-scrolls to the latest line
- Use `PageUp`/`PageDown` to scroll through history

### Screens

| Screen | Description | Launched from |
|---|---|---|
| **Startup** | Main menu: New, Open, Recent, Templates, Settings, Quit | Launch |
| **Wizard** | 5-step new project creator | Startup → New Project |
| **Settings** | Read-only view of current configuration | Startup → Settings |
| **Main Workspace** | Three-panel project management | After Generate |

### Overlays (Workspace only)

| Overlay | Key | Description |
|---|---|---|
| **Build** | `b` | Shows build progress, logs, cancel option |
| **Run** | `r` | Launches executable with argument prompts |
| **Dependencies** | `d` | Check/uncheck dependency presets |
| **Help** | `h` | Keybinding reference |
| **Conflict** | (auto) | Hand-edit resolution (View Diff / Overwrite / Keep Mine) |

---

## ⚙️ Configuration

Config files are JSON, resolved in 5 layers (later overrides earlier):

| # | Layer | Location | Purpose |
|---|---|---|---|
| 1 | Built-in defaults | Compiled into binary | Sensible defaults |
| 2 | System-wide | `/etc/lazycmake/` | Admin-managed config |
| 3 | User | `~/.config/lazycmake/` | Personal preferences |
| 4 | Project | `<root>/.lazycmake/` | Per-project overrides |
| 5 | CLI | `--setting=value` | One-off overrides (future) |

### Settings (`~/.config/lazycmake/settings.json`)

```json
{
    "general": {
        "language": "en",
        "checkUpdates": false,
        "restoreLastProject": true
    },
    "build": {
        "parallelJobs": 4,
        "exportCompileCommands": true,
        "autoConfigure": true,
        "buildTimeoutSec": 120,
        "verboseBuild": false
    },
    "editor": {
        "editorCommand": "",
        "autoSaveBeforeEdit": true
    },
    "tui": {
        "showLineNumbers": true,
        "mouseSupport": true,
        "panelSplitPercent": 33
    }
}
```

### Key Bindings (`~/.config/lazycmake/keymap.json`)

```json
{
    "global": {
        "quit": "q",
        "help": "h"
    },
    "startup": {
        "navigateUp": "k",
        "navigateDown": "j",
        "select": "enter"
    },
    "main_workspace": {
        "build": "b",
        "run": "r",
        "dependencies": "d"
    },
    "wizard": {
        "next": "tab",
        "back": "shift+tab",
        "generate": "ctrl+g"
    }
}
```

### Themes (`~/.config/lazycmake/themes/my-theme.json`)

```json
{
    "name": "my-theme",
    "colors": {
        "background": "#1a1b26",
        "foreground": "#a9b1d6",
        "accent": "#7aa2f7",
        "error": "#f7768e",
        "warning": "#e0af68",
        "info": "#7dcfff",
        "panelBackground": "#1a1b26",
        "panelBorderFocused": "#7aa2f7",
        "panelBorderUnfocused": "#3b4261",
        "listSelectionBackground": "#7aa2f7",
        "listSelectionForeground": "#1a1b26",
        "listInactiveBackground": "#1a1b26",
        "listInactiveForeground": "#565f89"
    }
}
```

---

## 📦 Dependencies

### Built-in Presets

| Name | Source | Description |
|---|---|---|
| `fmt` | FetchContent | Modern C++ formatting library |
| `spdlog` | FetchContent | Fast, header-only logging |
| `nlohmann_json` | FetchContent | JSON for modern C++ |
| `Catch2` | FetchContent | Unit testing framework |
| `Boost.Asio` | FetchContent | Networking & async I/O |
| `SDL3` | FetchContent | Cross-platform graphics/windowing |
| `GLFW` | FetchContent | Windowing & OpenGL context |
| `ImGui` | FetchContent | Dear ImGui GUI library |
| `FTXUI` | FetchContent | Terminal UI library (also used internally) |
| `EnTT` | FetchContent | ECS game engine |

### Custom Presets

Add user presets in `~/.config/lazycmake/dependencies.json`:

```json
{
    "my-library": {
        "description": "My in-house networking library",
        "gitRepo": "https://github.com/me/my-library.git",
        "gitTag": "v2.1.0",
        "linkTargets": ["my-library::my-library"]
    }
}
```

---

## 🔌 Plugins

Plugins are shared libraries placed in `~/.config/lazycmake/plugins/` with a manifest.

### Building the Example Plugin

```bash
cd plugins/clang_format
mkdir -p build && cd build
cmake .. -DCMAKE_PREFIX_PATH=/path/to/lazycmake/build
cmake --build .
mkdir -p ~/.config/lazycmake/plugins/
cp liblazycmake_clang_format.so ~/.config/lazycmake/plugins/
```

### Manifest (`manifest.json`)

```json
{
    "name": "clang-format",
    "version": "1.0.0",
    "description": "Auto-format sources on save",
    "author": "LazyCMake Contributors",
    "enabled": true,
    "library": "liblazycmake_clang_format.so",
    "apiVersion": 1
}
```

### Writing a Plugin

```cpp
#include <lazycmake/plugins/plugin_api.hpp>

class MyPlugin : public lazycmake::plugins::IPlugin {
public:
    const char* name() const override { return "my-plugin"; }
    const char* version() const override { return "1.0.0"; }

    bool onLoad(const lazycmake::plugins::PluginContext& ctx) override {
        ctx.eventBus->subscribe<lazycmake::events::BuildFinishedEvent>(
            [](const auto& e) {
                // React to build completion
            });
        return true;
    }

    void onUnload() override {}
};

// C-ABI entry point
extern "C" LAZYCMAKE_PLUGIN_EXPORT
const lazycmake::plugins::PluginDescriptor* lazycmake_plugin_descriptor() {
    static MyPlugin plugin;
    static lazycmake::plugins::PluginDescriptor desc = {
        .apiVersion = 1,
        .plugin = &plugin
    };
    return &desc;
}
```

---

## 🔍 Hand-Edit Sync

LazyCMake tracks generated files via `.lazycmake/generated.lock.json` (SHA256 hashes).

When regenerating, it checks each file:

1. **Hash matches** → safe to overwrite
2. **File doesn't exist** → create it
3. **Hash differs** (user edited externally) → **Conflict Dialog**:

```
╔══════════════════════════════════════════════════════════╗
║                  File Conflict Detected                   ║
║                                                          ║
║   CMakeLists.txt was edited outside LazyCMake.            ║
║                                                          ║
║   [1] View Diff — see what changed                       ║
║   [2] Overwrite — replace (backup as .bak)               ║
║   [3] Keep Mine & Stop Tracking                          ║
║                                                          ║
║   Choose: 1/2/3   esc: cancel                            ║
╚══════════════════════════════════════════════════════════╝
```

---

## 📋 Project Templates

Place templates in `~/.config/lazycmake/templates/`:

```
~/.config/lazycmake/templates/my-template/
├── template.json
├── src/main.cpp.tmpl
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

Templates support `{{placeholder}}` substitution — the wizard prompts for values.

---

## 🏗 Architecture

### Layer Diagram

```
┌────────────────────────────────────────────────────────────────────┐
│                        TUI Layer (src/tui/)                        │
│  Application  │  screens/  │  panels/  │  overlays/  │  helpers/   │
│  Navigation   │  Startup   │  File     │  Build       │  Color      │
│  ScreenStack  │  Wizard    │  Target   │  Run         │  Themes     │
│               │  Settings  │  Output   │  Dependencies│             │
│               │  Workspace │           │  Conflict    │             │
├────────────────────────────────────────────────────────────────────┤
│                     EventBus (typed pub/sub)                       │
│  ProjectLoaded  │  BuildStarted  │  RunOutput  │  FileAdded        │
│  ConfigReloaded │  BuildFinished │  RunFinished │  PluginLoaded     │
├────────┬────────┬────────┬────────┬────────┬────────┬──────────────┤
│  Core  │ CMake  │ Config │ Build  │ Events │ Plugin │  FsWatch     │
│  Model │ Gen    │ System │ Engine │  Bus   │  Host  │              │
│  (§6)  │ (§10)  │ (§15)  │ (§11)  │  (§8)  │ (§16)  │  (§12)      │
├────────┴────────┴────────┴────────┴────────┴────────┴──────────────┤
│                    External Libraries                               │
│  nlohmann_json  │  fmt  │  spdlog  │  OpenSSL  │  FTXUI (TUI only) │
└────────────────────────────────────────────────────────────────────┘
```

### Key Libraries

| Target | Description | Headers | Dependencies |
|---|---|---|---|
| `lazycmake_core` | Domain logic, generators, config, events, build, plugins, watcher | `include/lazycmake/{core,cmake_gen,config,events,build,fswatch,plugins}/` | nlohmann_json, fmt, spdlog, OpenSSL |
| `lazycmake_tui` | FTXUI-based terminal interface | `include/lazycmake/tui/` | lazycmake_core, FTXUI |
| `lazycmake` | Main executable | `src/tui/main.cpp` | lazycmake_tui |

### Boundary Rule

Only `src/tui/` and `include/lazycmake/tui/` may `#include` FTXUI headers — enforced by a configure-time grep lint. The core library compiles and tests cleanly without any terminal dependency.

---

## 🧪 Testing

```bash
# Run full suite (102 test cases, 339 assertions)
./build/tests/lazycmake_core_tests

# Run by test case name
./build/tests/lazycmake_core_tests "EventBus"
./build/tests/lazycmake_core_tests "DependencyRegistry"
./build/tests/lazycmake_core_tests "ModernCMakeGenerator"

# Run examples
./build/examples/example01_basic_project_generation
./build/examples/example02_event_bus_usage
./build/examples/example03_config_system_demo
./build/examples/example04_build_pipeline_demo
```

### Test Coverage by Layer

| Layer | Test Cases | Assertions |
|---|---|---|
| Project model (types, JSON roundtrip) | ✓ | ✓ |
| CMake generator | ✓ | ✓ |
| Project repository (load/save) | ✓ | ✓ |
| Configuration system | ✓ | ✓ |
| EventBus | ✓ | ✓ |
| Build pipeline | ✓ | ✓ |
| Run manager | ✓ | ✓ |
| CMake importer | ✓ | ✓ |
| Generated file lock | ✓ | ✓ |
| Dependency registry | ✓ | ✓ |
| File watcher | ✓ | ✓ |
| Diagnostic parser | ✓ | ✓ |
| Target validator | ✓ | ✓ |

---

## 📦 Packaging

```bash
# Build with TUI
cmake -S . -B build -DLAZYCMAKE_BUILD_TUI=ON
cmake --build build -j$(nproc)

# Package
cpack --config build/CPackConfig.cmake

# Generated artifacts:
#   build/LazyCMake-0.1.0-Linux.tar.gz
#   build/LazyCMake-0.1.0-Linux.deb
#   build/LazyCMake-0.1.0-Linux.rpm

# Install to system
cmake --install build --prefix /usr/local
```

---

## 🗺 Project Map

```
├── CMakeLists.txt              # Root build — controls all targets
├── README.md                   # This file
├── LICENSE                     # MIT license
├── docs/
│   └── DESIGN.md               # Full design document (§1–§18)
├── cmake/
│   └── LazyCMakeConfig.cmake   # find_package(LazyCMake) support
├── include/lazycmake/
│   ├── core/                   # Project types, validation, repository
│   ├── cmake_gen/              # CMake AST, printer, ModernCMakeGenerator
│   ├── config/                 # Settings, Keymap, Theme, Template managers
│   ├── events/                 # EventBus, PluginEventBus
│   ├── build/                  # BuildBackend, BuildManager, RunManager, DiagnosticParser
│   ├── fswatch/                # FileWatcher, FileClassifier
│   ├── plugins/                # Plugin API, descriptor, host
│   └── tui/                    # Application, screens, overlays, panels
├── src/
│   ├── core/                   # Domain logic implementations
│   ├── cmake_gen/              # Generator implementations
│   ├── config/                 # Config loader implementations
│   ├── events/                 # EventBus implementation
│   ├── build/                  # Build/Run engine implementations
│   ├── fswatch/                # File watcher implementation
│   ├── plugins/                # Plugin host implementation
│   └── tui/                    # TUI implementations
│       ├── panels/             # FilePanel, TargetPanel, OutputPanel
│       └── main.cpp            # TUI entry point
├── tests/                      # Catch2 test suite (102 test cases)
├── examples/                   # 4 usage examples (one per major layer)
├── plugins/
│   └── clang_format/           # Example plugin with manifest
├── resources/                  # Built-in templates and themes
└── build/                      # CMake build output
```

---

## ⌨️ Keyboard Reference

### Global

| Key | Action |
|---|---|
| `q` | Quit / close current |
| `h` | Toggle help overlay |
| `Escape` | Close overlay / go back |

### Startup Screen

| Key | Action |
|---|---|
| `j` / `↓` | Move down |
| `k` / `↑` | Move up |
| `Enter` | Select item |

### Main Workspace

| Key | Action |
|---|---|
| `j` / `↓` | Move down in panel |
| `k` / `↑` | Move up in panel |
| `Tab` | Next panel (Files → Targets → Output) |
| `Shift+Tab` | Previous panel |
| `b` | Open build overlay |
| `r` | Open run overlay |
| `d` | Open dependency dialog |
| `PageUp` | Scroll output up |
| `PageDown` | Scroll output down |

### Wizard

| Key | Action |
|---|---|
| `Tab` | Next field |
| `Enter` | Confirm / Next |
| `Escape` | Cancel |
| `←` / `→` | Toggle between options |

### Overlays

| Key | Action |
|---|---|
| `Escape` | Close overlay |
| `Space` | Toggle checkbox (dependencies) |
| `1`/`2`/`3` | Conflict resolution choice |

---

## 🎯 Design Principles

1. **Zero manual CMake required** — every UI action maps to a generator operation; you never need to touch CMakeLists.txt
2. **Nothing is hidden** — generated files are always on disk, human-readable, and independently re-generatable
3. **Hand-edits are detected, never silently overwritten** — SHA256 hash tracking with conflict resolution
4. **Open for extension** — plugins use the same public API as built-in features
5. **Testable core** — pure domain logic with zero UI dependency; all 102 test cases run without a terminal
6. **Layered config** — no magic; every setting is traceable to its source file

---

## 📄 License

MIT — see [LICENSE](LICENSE). Built with [FTXUI](https://github.com/ArthurSonzogni/FTXUI), [fmt](https://fmt.dev), [spdlog](https://github.com/gabime/spdlog), [nlohmann_json](https://json.nlohmann.me), [Catch2](https://github.com/catchorg/Catch2).
