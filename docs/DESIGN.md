# LazyCMake — Software Design Document & Implementation Plan

> "lazygit, but for CMake."

Version: 0.1 (pre-implementation)
Status: Design complete, Core layer implementation in progress

---

## Table of Contents

1. [Vision & Design Philosophy](#1-vision--design-philosophy)
2. [Customization as a First-Class Concern](#2-customization-as-a-first-class-concern)
3. [Technology Stack](#3-technology-stack)
4. [Directory Structure](#4-directory-structure)
5. [Layered Architecture](#5-layered-architecture)
6. [Core Data Models](#6-core-data-models)
7. [Class Design](#7-class-design)
8. [Event System](#8-event-system)
9. [State Machines](#9-state-machines)
10. [CMake Generator Design](#10-cmake-generator-design)
11. [Build Engine Design](#11-build-engine-design)
12. [File Watcher Design](#12-file-watcher-design)
13. [Dependency Manager Design](#13-dependency-manager-design)
14. [TUI Layer Design](#14-tui-layer-design)
15. [Configuration System](#15-configuration-system)
16. [Plugin System](#16-plugin-system)
17. [UI Flow Diagrams](#17-ui-flow-diagrams)
18. [Testing Strategy](#18-testing-strategy)
19. [Implementation Roadmap](#19-implementation-roadmap)
20. [Stretch Goals — Design Hooks](#20-stretch-goals--design-hooks)

---

## 1. Vision & Design Philosophy

LazyCMake is a terminal application that lets a developer create, configure,
build, and run CMake-based C/C++ projects without ever hand-writing a
`CMakeLists.txt`. It targets the same niche `lazygit` fills for Git: a fast,
keyboard-and-mouse-driven TUI that makes a powerful-but-fiddly tool
approachable, while never hiding what it does.

Non-negotiable principles, carried through every layer of this design:

- **Zero manual CMake required** — every action in the UI maps to a
  generator operation; the user never *has* to open a text editor.
- **Nothing is hidden.** Generated `CMakeLists.txt` / `*.cmake` files are
  always written to disk, always human-readable, always re-generatable.
  Hand-edits are detected, not overwritten silently (see §10.4).
- **Terminal-first, but not terminal-only-because-we-have-to.** Mouse and
  keyboard are both first-class. A new user can drive the whole app with a
  mouse; an expert never has to touch one.
- **Fast for experts, safe for beginners.** Destructive actions (delete
  file, remove target, overwrite hand-edited CMake) always confirm. Common
  actions are single keystrokes.
- **Separation of UI and logic, total.** The Core layer has no dependency
  on FTXUI, or on any terminal concept whatsoever. It could be driven by a
  CLI, a web UI, or a test harness with zero changes. This is the
  architectural backbone that also makes the system *customizable* (§2).

---

## 2. Customization as a First-Class Concern

Customization is treated as two related but distinct surfaces, and the
architecture supports both equally rather than picking one:

### 2.1 User-level customization (no code, no rebuild)

Everything a user is likely to want to tweak lives in **plain data files**,
loaded at runtime, layered, and hot-reloadable:

| Surface | Format | Location | Hot-reload |
|---|---|---|---|
| Keybindings | TOML | `~/.config/lazycmake/keymap.toml` | Yes (§15.3) |
| Color themes | TOML | `~/.config/lazycmake/themes/*.toml` | Yes |
| Project templates | JSON + Mustache-style files | `~/.config/lazycmake/templates/<name>/` | On wizard open |
| Dependency presets | JSON | `~/.config/lazycmake/dependencies.json` | On dialog open |
| App settings | TOML | `~/.config/lazycmake/settings.toml` | Partial (some need restart) |
| Plugin manifests | TOML | `~/.config/lazycmake/plugins/*.toml` | On startup |

All of these ship with built-in defaults compiled into the binary as a
fallback (`resources/` embedded via `cmake -E` resource baking, see §4), so
the app works with zero config files present, but every file is just a
layer the user can override, in a clearly documented **layered config
resolution order** (built-in defaults → system config → user config →
project-local `.lazycmake/` overrides → CLI flags). This mirrors how Git or
ripgrep resolve config, which developers already understand.

### 2.2 Developer-level / code-level extensibility

For anyone extending LazyCMake itself (or writing a plugin in C++), the
architecture exposes clean seams rather than requiring forks:

- **Plugin API** (§16) — a stable, versioned C++ ABI-safe interface
  (`IPlugin`, `IBuildStep`, `IDependencyProvider`, `IPanelProvider`) that
  external `.so`/`.dll`/`.dylib` plugins implement. Plugins can:
  - inject new build steps (e.g. clang-tidy, Doxygen)
  - register new dependency providers (Conan, additional vcpkg registries)
  - register new TUI panels/screens
  - register new project templates
  - hook lifecycle events (pre-configure, post-build, file-added, etc.)
- **Strategy-pattern internals** — `CMakeGenerator`, `BuildSystemBackend`,
  `DependencyResolver`, and `ThemeRenderer` are all interfaces with at least
  two built-in implementations, so a contributor adding (say) a Bazel
  backend or a Meson export does not touch existing code (Open/Closed
  Principle, see §7).
- **Event bus, not callbacks** — all cross-layer communication goes through
  a typed pub/sub event bus (§8). Plugins and core modules subscribe the
  same way core UI panels do. There is no privileged internal API that
  plugins are locked out of.

The unifying idea: **the config-file surface and the plugin surface are
two views onto the same extension points.** A "dependency preset" in JSON
and an `IDependencyProvider` plugin both ultimately populate the same
`DependencyRegistry`. A "theme" TOML file and a hypothetical
`IThemeProvider` plugin both populate the same `ThemeRegistry`. This keeps
the system from growing two incompatible customization stories over time.

---

## 3. Technology Stack

| Concern | Choice | Why |
|---|---|---|
| Language | C++20 | Concepts, ranges, `std::filesystem`, designated initializers help readability of generator code |
| TUI | [FTXUI](https://github.com/ArthurSonzogni/FTXUI) | Mouse + keyboard, flexbox-like layout, active maintenance, header-only-friendly |
| JSON | nlohmann/json | De-facto standard, ergonomic, used for config + plugin manifests |
| TOML | toml++ (`tomlplusplus`) | Used for human-edited config (more forgiving syntax for hand-editing than JSON) |
| Formatting | fmt | Used everywhere instead of iostream/printf |
| Logging | spdlog | Structured logs to `~/.local/state/lazycmake/log/`, also pluggable sink for an in-app "Log" panel |
| Filesystem | `std::filesystem` | No extra dependency needed in C++20 |
| Process exec | `reproc` (or `boost.process` fallback) | Cross-platform async subprocess spawning, needed for non-blocking builds |
| File watching | `efsw` (Efficient File Watcher) | Cross-platform native watcher (inotify/FSEvents/ReadDirectoryChangesW) |
| Build | CMake + Ninja (self-hosted) | Dogfooding; Makefiles supported as fallback generator |
| Testing | Catch2 v3 | Matches "unit-testable core" requirement; BDD-style sections read well for state-machine tests |
| Plugin loading | Native dynamic loading (`dlopen`/`LoadLibrary`) wrapped in a small cross-platform shim | Avoids a heavy plugin framework dependency |

All dependencies are themselves fetched via CMake `FetchContent` with
pinned tags, so LazyCMake's own build is a working example of the
dependency story it gives to users (§13).

---

## 4. Directory Structure

```
lazycmake/
├── CMakeLists.txt
├── CMakePresets.json
├── docs/
│   ├── DESIGN.md                 (this document)
│   ├── diagrams/
│   └── adr/                      Architecture Decision Records
├── include/lazycmake/            Public headers (stable ABI surface)
│   ├── core/                     Project, Target, Dependency, BuildConfig...
│   ├── cmake_gen/                ICMakeGenerator + ModernCMakeGenerator
│   ├── build/                    IBuildBackend, BuildManager, RunManager
│   ├── config/                   SettingsManager, KeymapManager, ThemeManager
│   ├── plugins/                  IPlugin, IBuildStep, IDependencyProvider, PluginHost
│   └── events/                   EventBus, Event variant types
├── src/
│   ├── core/                     Implementations, no FTXUI dependency, no I/O side effects beyond fs
│   ├── cmake_gen/
│   ├── build/
│   ├── fswatch/
│   ├── config/
│   ├── plugins/
│   └── tui/                      ALL FTXUI code lives here and only here
├── resources/                    Embedded defaults (baked into binary)
│   ├── themes/                   dark.toml, nord.toml, dracula.toml
│   ├── templates/                default project templates
│   └── keymaps/                  default.toml (vim-like)
├── plugins/
│   └── examples/                 Example out-of-tree plugins (clang-format, ctest)
├── tests/
│   ├── core/
│   └── cmake_gen/
└── third_party/                  FetchContent cache (gitignored)
```

**Key boundary:** `src/tui/` is the *only* directory permitted to
`#include` any FTXUI header. This is enforced by a CI lint step (a simple
grep over `src/core`, `src/cmake_gen`, `src/build`, `src/config`,
`src/plugins` for `ftxui/`) — cheap, but it's the single rule that keeps
Core UI-framework-agnostic and testable without a terminal.

---

## 5. Layered Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                         TUI Layer                            │
│   FTXUI screens/components, input handling, rendering        │
│   (KeymapManager translation, ThemeManager application)      │
└───────────────────────────┬───────────────────────────────────┘
                            │ Event Bus (pub/sub, §8)
┌───────────────────────────▼───────────────────────────────────┐
│                      Application Layer                       │
│   Application (top-level orchestrator), Command dispatch     │
└──────┬─────────┬─────────┬─────────┬─────────┬───────────────┘
       │         │         │         │         │
┌──────▼───┐ ┌────▼────┐ ┌──▼─────┐ ┌─▼──────┐ ┌▼─────────────┐
│ Project  │ │ CMake   │ │ Build  │ │ File   │ │ Dependency   │
│ Model    │ │Generator│ │ Engine │ │Watcher │ │ Manager      │
│ (Core)   │ │         │ │        │ │        │ │              │
└──────┬───┘ └────┬────┘ └──┬─────┘ └─┬──────┘ └┬─────────────┘
       │          │         │         │          │
       └──────────┴────┬────┴─────────┴──────────┘
                        │
              ┌─────────▼─────────┐
              │  Plugin Host       │   loads .so/.dll, exposes
              │  (Plugin Layer)    │   IPlugin hooks into every
              └─────────┬─────────┘   layer above via Event Bus
                        │
              ┌─────────▼─────────┐
              │ Configuration      │  Settings / Keymap / Theme
              │ System             │  managers — layered file
              └────────────────────┘  resolution (§2.1, §15)
```

Dependency direction is strictly downward/inward: TUI depends on
Application depends on Core services; nothing in Core depends upward. The
Plugin Host and Configuration System are depended on by everything but
depend on nothing above them — this is what makes both customization
surfaces (§2) reachable from every layer without circular includes.

---

## 6. Core Data Models

All Core data models are plain value-like structs with no UI or I/O
dependencies, serializable to/from JSON for both the on-disk project
manifest (`.lazycmake/project.json`) and plugin communication.

```cpp
// include/lazycmake/core/types.hpp

enum class Language   { C, Cpp, Both };
enum class CppStandard { Cpp17, Cpp20, Cpp23 };
enum class CStandard   { C11, C17 };
enum class TargetKind  { Executable, StaticLibrary, SharedLibrary, InterfaceLibrary };
enum class BuildType   { Debug, Release, RelWithDebInfo, MinSizeRel };
enum class GeneratorKind { Ninja, UnixMakefiles, NMake };

enum class DependencySource { FetchContent, CPM, VcpkgFindPackage, SystemFindPackage };

struct DependencySpec {
    std::string        name;            // "fmt", "SDL3", "ImGui"
    DependencySource    source;
    std::string        gitRepo;          // for FetchContent/CPM
    std::string        gitTag;
    std::vector<std::string> linkTargets; // e.g. {"fmt::fmt"}
    bool                enabled = false;
};

struct SourceFileRef {
    std::filesystem::path path;          // relative to project root
    bool                  autoDetected;  // added by FileWatcher vs user-added
};

struct Target {
    std::string                 name;
    TargetKind                  kind;
    std::vector<SourceFileRef>  sources;
    std::vector<std::filesystem::path> includeDirs;
    std::vector<std::string>    linkTargets;     // names of other Targets or DependencySpec link names
    std::optional<CppStandard>  cppStandard;     // overrides Project default if set
    std::map<std::string, std::string> compileDefinitions;
};

struct BuildConfig {
    GeneratorKind generator = GeneratorKind::Ninja;
    BuildType     defaultBuildType = BuildType::Debug;
    std::string   compilerOverride;     // empty = use CMake default detection
    std::filesystem::path buildDir = "build";
};

struct Project {
    std::string               name;
    Language                  language;
    std::optional<CppStandard> cppStandard;
    std::optional<CStandard>   cStandard;
    std::filesystem::path     rootDir;
    std::vector<Target>       targets;
    std::vector<DependencySpec> dependencies;
    BuildConfig                buildConfig;
    std::string                schemaVersion = "1.0";   // for migration on load
};
```

This `Project` struct is the **single source of truth**. The CMake
Generator (§10) is a pure function of `Project -> CMakeLists.txt text`; the
TUI never edits `CMakeLists.txt` text directly, it always edits the
`Project` model and re-invokes the generator. This is what guarantees
"generated CMake is never hand-patched by the app" and what makes
hand-edit detection (§10.4) tractable — the app always knows what it
*would* generate and can diff against what's on disk.

---

## 7. Class Design

### 7.1 Core layer

| Class | Responsibility | Notes |
|---|---|---|
| `Project` | Data model (§6) | Pure data, no behavior beyond validation |
| `ProjectRepository` | Load/save `Project` ↔ `.lazycmake/project.json`, schema migration | Only place that touches the manifest file |
| `TargetValidator` | Cross-target validation (dup names, cyclic links, missing sources) | Pure function, easy to unit test |
| `DependencyRegistry` | Holds known `DependencySpec` presets, merged from built-in + user JSON + plugins | Implements the "two views, one registry" idea from §2.2 |

### 7.2 Generation & build layer

| Class | Responsibility | Notes |
|---|---|---|
| `ICMakeGenerator` (interface) | `generate(const Project&) -> GeneratedFileSet` | Strategy pattern seam for alternate generators |
| `ModernCMakeGenerator` | Default impl: target-based, modern CMake only (§10) | |
| `GeneratedFileSet` | In-memory map of relative path → file content, pre-write | Lets the diff/hand-edit-detection logic run before touching disk |
| `IBuildBackend` (interface) | `configure()`, `build(target)`, async, cancellable | |
| `NinjaBackend`, `MakeBackend` | Concrete backends, wrap subprocess execution via `reproc` | |
| `BuildManager` | Orchestrates Configure → Generate → Build → Link state machine (§9.1), owns async task, emits `BuildEvent`s | |
| `RunManager` | Launches built executable with args/env/cwd, streams stdout/stderr, captures exit code | |

### 7.3 Watching & dependencies

| Class | Responsibility | Notes |
|---|---|---|
| `FileWatcher` | Wraps `efsw`, debounces, emits `FileSystemEvent` | Translates raw fs events into "is this a source file LazyCMake should know about" |
| `FileClassifier` | Decides if a changed path matters (extension allow-list, ignore patterns from `.lazycmakeignore`) | Pure function |
| `DependencyResolver` | Given enabled `DependencySpec`s, decides FetchContent/CPM/vcpkg fragments to emit | Delegates to `IDependencyProvider` plugins (§16) for non-built-in sources |

### 7.4 Configuration layer (§15)

| Class | Responsibility |
|---|---|
| `SettingsManager` | Loads/merges `settings.toml` layers, exposes typed getters, hot-reload via `FileWatcher` on its own config dir |
| `KeymapManager` | Loads `keymap.toml` layers, resolves a `KeyEvent -> Action` mapping, exposes "what is bound to X" for the Help overlay |
| `ThemeManager` | Loads `themes/*.toml`, exposes an FTXUI `Decorator`/color palette to the TUI layer |
| `TemplateManager` | Discovers built-in + user templates for the New Project wizard |

### 7.5 Plugin layer (§16)

| Class | Responsibility |
|---|---|
| `PluginHost` | Discovers, loads, version-checks, and initializes plugins; owns their lifetime |
| `IPlugin` | Base interface every plugin implements (`onLoad`, `onUnload`, `apiVersion`) |
| `IBuildStep` | Plugin-contributed pipeline step (pre/post configure, pre/post build) |
| `IDependencyProvider` | Plugin-contributed dependency source (e.g. Conan) |
| `IPanelProvider` | Plugin-contributed TUI panel (registered into the TUI layer's panel registry) |

### 7.6 Application & TUI layer

| Class | Responsibility |
|---|---|
| `Application` | Top-level orchestrator: owns `Project`, all managers, the `EventBus`, and the `PluginHost`; has zero FTXUI knowledge |
| `Screen` (FTXUI-facing base) | Base for Startup/Wizard/MainWorkspace/Settings screens |
| `MainWorkspace` | The three-panel Files/Targets/Output layout |
| `HelpOverlay` | Renders current `KeymapManager` bindings + focused panel + project status |
| `CommandDispatcher` | Translates resolved `Action` (from KeymapManager) into calls against `Application` |

This table maps 1:1 onto the class list given in the original brief
(`Project`, `Target`, `Dependency`, `BuildManager`, `RunManager`,
`FileWatcher`, `CMakeGenerator`, `SettingsManager`, `KeymapManager`,
`HelpOverlay`, `Application`) while adding the seams needed for
testability and plugin extension (`ICMakeGenerator`/`IBuildBackend`
interfaces, `ProjectRepository`, `DependencyRegistry`, `PluginHost`).

---

## 8. Event System

All cross-layer communication — including plugin hooks — goes through a
single typed event bus rather than direct method calls or callback
registration scattered across classes. This keeps the Plugin Host and the
TUI layer on equal footing (§2.2): a plugin subscribes to `BuildEvent`
the same way the Build screen does.

```cpp
// include/lazycmake/events/event_bus.hpp

using Event = std::variant<
    ProjectLoadedEvent, ProjectSavedEvent,
    ConfigureStartedEvent, ConfigureFinishedEvent,
    BuildStartedEvent, BuildProgressEvent, BuildFinishedEvent,
    RunStartedEvent, RunOutputEvent, RunFinishedEvent,
    FileAddedEvent, FileRemovedEvent, FileRenamedEvent,
    DependencyToggledEvent,
    ConfigReloadedEvent,
    PluginLoadedEvent, PluginErrorEvent
>;

class EventBus {
public:
    using HandlerId = std::uint64_t;

    template <typename T>
    HandlerId subscribe(std::function<void(const T&)> handler);

    void unsubscribe(HandlerId id);

    void publish(Event event);   // synchronous dispatch on the calling thread

    // Async-origin events (build output, fs watcher) are published via
    // a thread-safe queue and drained on the UI thread once per frame.
    void publishThreadSafe(Event event);
    void drainQueue();           // called once per FTXUI render loop tick
};
```

Design notes:

- **Single-threaded delivery to the UI.** Background threads (build
  process, file watcher) only ever call `publishThreadSafe`, which pushes
  onto a lock-protected queue. `Application::tick()` drains it on the main
  thread before each FTXUI render pass — this means subscribers (including
  plugins) never need their own locking.
- **Typed, not stringly.** Using a `std::variant` instead of a string topic
  + `void*` payload means plugin authors get compiler errors, not runtime
  surprises, when the event shape changes between versions (checked via
  `apiVersion()`, §16.2).
- **Plugins subscribe through `PluginHost`**, which holds the real
  `EventBus` reference and exposes a restricted facade
  (`PluginEventBus`) that only allows subscribing, not publishing
  arbitrary core events — a plugin can *react* to a build finishing, but
  cannot pretend to *be* the build engine and forge a `BuildFinishedEvent`.

---

## 9. State Machines

### 9.1 Build Pipeline State Machine

```
        ┌─────────┐  configure()   ┌────────────┐
        │  Idle   ├───────────────►│ Configuring│
        └────▲────┘                └─────┬──────┘
             │                            │ success
             │ cancel()/error             ▼
             │                     ┌────────────┐
             │                     │ Generating │   (CMake re-gen if
             │                     └─────┬──────┘    Project model dirty)
             │                            │ success
             │                            ▼
             │                     ┌────────────┐
             ├─────────────────────┤  Building  │
             │      error          └─────┬──────┘
             │                            │ success
             │                            ▼
             │                     ┌────────────┐
             └─────────────────────┤  Linking   │
                   error            └─────┬──────┘
                                            │ success
                                            ▼
                                     ┌────────────┐
                                     │  Succeeded │──► (user presses r) ──► Run state machine (9.2)
                                     └────────────┘
```

Every transition emits a `BuildProgressEvent{stage, status}` consumed by
the Build Screen's checklist UI (`✓ Configure  ✓ Generate  ⠋ Build  · Link`)
and by any plugin implementing `IBuildStep` for that stage. Errors at any
stage transition to a terminal `Failed(stage)` state that the UI renders
with the scrollable error pane; `Idle` is only re-entered by an explicit
retry/edit action, never automatically, so the user always sees what
failed.

### 9.2 Run State Machine

```
Idle ─ run() ─► Launching ─► Running ─┬─ exit(code) ─► Finished
                                       └─ kill()      ─► Killed
```

`RunManager` streams `RunOutputEvent{stream: stdout|stderr, line}` while in
`Running`; the Run Screen appends to two scroll buffers live.

### 9.3 File Watcher → "Add to target?" State Machine

```
Watching ─ new file detected ─► PendingConfirmation ─┬─ Y ─► target picker ─► Watching
                                                       └─ N ─► ignored-list (session) ─► Watching
```

`ignored-list` is **session-scoped** by default (re-asks next launch)
unless the user also toggles "remember for this project," which persists
the ignore into `.lazycmake/ignore.json` — chosen deliberately so a
beginner who mis-clicks "No" once isn't permanently stuck.

---

## 10. CMake Generator Design

### 10.1 Principles

- Always target-based (`target_include_directories`, `target_link_libraries`,
  `target_compile_features`). Never `include_directories()`,
  `link_libraries()`, or global `set(CMAKE_CXX_STANDARD ...)` — the brief's
  "never generate legacy CMake" requirement is enforced structurally: the
  generator's internal AST type (§10.2) has no node type that *can*
  represent a directory-scoped legacy call.
- One generated root `CMakeLists.txt` + one generated `<target>/CMakeLists.txt`
  per target via `add_subdirectory`, mirroring how most hand-written modern
  CMake projects are organized — this also keeps re-generation diffs small
  (changing one target doesn't rewrite the whole tree's text).
- Debug/Release handled via CMake's native multi-config awareness
  (`$<CONFIG:Debug>` generator expressions for config-specific defines)
  rather than duplicated `if(CMAKE_BUILD_TYPE STREQUAL "Debug")` blocks.

### 10.2 Internal representation

The generator does **not** do string concatenation directly against
`Project`. It first lowers `Project` into a small CMake AST
(`CMakeFile { vector<CMakeStatement> }`), then a separate `CMakePrinter`
renders that AST to text. This split exists specifically for
customization (§2.2): a plugin or a contributor can post-process the AST
(e.g. an `IBuildStep`-adjacent hook that injects an extra
`target_compile_options` statement for a sanitizer profile) without
string-patching generated text.

```cpp
struct ICMakeGenerator {
    virtual GeneratedFileSet generate(const Project&) const = 0;
    virtual ~ICMakeGenerator() = default;
};

class ModernCMakeGenerator : public ICMakeGenerator {
public:
    GeneratedFileSet generate(const Project& project) const override;
private:
    CMakeFile buildRootFile(const Project&) const;
    CMakeFile buildTargetFile(const Project&, const Target&) const;
    CMakeFile buildDependenciesFile(const Project&) const; // FetchContent/CPM block
};
```

### 10.3 Example output shape (illustrative, not final formatting)

```cmake
# Generated by LazyCMake — root CMakeLists.txt
# Do not hand-edit the regions marked LAZYCMAKE:BEGIN/END — see docs.
cmake_minimum_required(VERSION 3.21)
project(MyGame LANGUAGES CXX)

include(cmake/dependencies.cmake)

add_subdirectory(engine)
add_subdirectory(app)
add_subdirectory(tests)
```

```cmake
# engine/CMakeLists.txt
add_library(engine STATIC
    src/Scene.cpp
    src/Renderer.cpp
)
target_include_directories(engine PUBLIC include)
target_compile_features(engine PUBLIC cxx_std_20)
target_link_libraries(engine PUBLIC fmt::fmt EnTT::EnTT)
```

### 10.4 Hand-edit detection (never silently overwrite)

Because `Project` is the source of truth, re-generation always knows what
it *would* write. Before writing, `ProjectRepository` compares the
on-disk file's hash (stored at last-generation time in
`.lazycmake/generated.lock.json`) against the file's current on-disk hash:

- **Unchanged since last generation** → safe to overwrite.
- **Changed since last generation** (user hand-edited) → do not overwrite;
  surface a TUI dialog: *"`engine/CMakeLists.txt` was edited outside
  LazyCMake. [View Diff] [Overwrite] [Keep Mine & Stop Tracking This File]."*
  Choosing "Keep Mine" marks that specific generated file as
  user-owned in `generated.lock.json`, and from then on LazyCMake leaves it
  alone entirely — a deliberate, explicit opt-out per file rather than a
  global all-or-nothing toggle, so the rest of the project stays
  auto-managed.

This is the mechanism that reconciles "zero manual CMake required" with
"never hide generated files, user can edit if desired": editing is always
allowed, it's just never silently clobbered.

---

## 11. Build Engine Design

- `BuildManager` drives the state machine in §9.1. It never blocks the UI
  thread: `IBuildBackend::build()` returns a `BuildHandle` (a
  cancellable future-like object); progress and completion are delivered
  exclusively via `publishThreadSafe` events drained on the next UI tick.
- Compiler output is captured line-by-line (not buffered to completion)
  so the Output panel streams live, matching the "show compiler output"
  + "scrollable errors" requirements. A lightweight regex-based
  `DiagnosticParser` (GCC/Clang/MSVC formats) tags each line as
  info/warning/error for syntax-highlighted display and for a future
  "jump to next error" keybinding.
- `NinjaBackend` and `MakeBackend` both implement `IBuildBackend`; adding
  a third (e.g. a hypothetical `MesonBackend`) requires no change to
  `BuildManager`, only a registration call in `Application` startup —
  another Open/Closed seam in service of customization.

---

## 12. File Watcher Design

- `efsw` watches the project root (excluding `build/`, `.git/`, and
  patterns from `.lazycmakeignore`, itself a customizable file).
- Raw OS events are debounced (250ms default, configurable in
  `settings.toml`) and passed through `FileClassifier`, which only cares
  about extensions relevant to the project's `Language` setting.
- A classified add/remove/rename produces a `FileAddedEvent` etc. on the
  bus; the TUI's confirmation dialog (§9.3) is just one subscriber. A
  plugin could subscribe to the same event to, e.g., auto-run
  clang-format on new files — again, no privileged internal API.

---

## 13. Dependency Manager Design

`DependencyRegistry` is populated at startup from three layers, merged in
order (later layers can add new presets or override metadata of
same-named ones, but never silently change a project's already-enabled
dependencies):

1. **Built-in presets** compiled into the binary (`fmt`, `spdlog`, `SDL3`,
   `GLFW`, `ImGui`, `Catch2`, ...) — matches the brief's example checklist.
2. **User presets** from `~/.config/lazycmake/dependencies.json` — lets a
   user add an in-house library once and have it show up in every future
   project's wizard.
3. **Plugin-contributed providers** (`IDependencyProvider`) — for sources
   that need more than static metadata, e.g. a Conan plugin that queries
   `conancenter` for available versions at dialog-open time.

Each `DependencySpec` records which `DependencySource` it uses
(FetchContent / CPM.cmake / vcpkg `find_package`), and
`buildDependenciesFile()` (§10.2) emits the right CMake fragment per
source — e.g. FetchContent block with pinned `GIT_TAG`, or a
`find_package(fmt CONFIG REQUIRED)` for vcpkg-resolved ones, with
`target_link_libraries` entries added automatically to any target that
enabled that dependency (matching "Generated CMake must automatically
include target_link_libraries").

---

## 14. TUI Layer Design

- Built on FTXUI's `Component` + `Screen` model. `Application` owns a
  `ScreenStack` (Startup → Wizard → MainWorkspace, with Settings/Help as
  overlay layers rather than stack pushes, so `esc` always has a single
  consistent meaning: close overlay, or pop stack if no overlay).
- **Three-panel `MainWorkspace`** matches the brief's layout exactly:
  Files | Targets | Output, implemented as three FTXUI `Component`s
  composed via `ResizableSplit` (drag-to-resize with the mouse, `tab` /
  `shift+tab` to cycle focus with the keyboard).
- **Mouse support** is native to FTXUI's component model (click, scroll,
  drag) — panel resize handles, list selection, and button activation
  all work out of the box; the only custom work is wiring `OnEvent` for
  drag-resize between the three panels.
- **All rendering goes through `ThemeManager`** — no component hardcodes
  a color; every component pulls from a `Theme` struct (background,
  foreground, accent, error, warning, success, panel-border-focused,
  panel-border-unfocused) so switching themes live needs zero per-component
  code changes.
- **All key handling goes through `KeymapManager`** — components don't
  match on raw `Event::Character('j')`; they resolve the raw event to a
  semantic `Action` enum (`MoveDown`, `Open`, `Build`, ...) via
  `KeymapManager::resolve(event, currentContext)`, then dispatch the
  `Action`. This indirection is *the* reason rebinding keys is "easy and
  possible" — there is exactly one place key-to-action mapping happens.

---

## 15. Configuration System

### 15.1 Layered resolution order

For every config surface (settings, keymap, theme, templates,
dependency presets), resolution follows the same precedence, low to high:

```
1. Compiled-in defaults (resources/, baked via CMake into the binary)
2. System-wide config      (/etc/lazycmake/...        on Linux, etc.)
3. User config             (~/.config/lazycmake/...)
4. Project-local overrides (<project>/.lazycmake/...)
5. CLI flags / env vars    (--theme=nord, LAZYCMAKE_KEYMAP=...)
```

Each layer is a **partial override**, not a full replacement — e.g. a
user's `keymap.toml` only needs to list the three bindings they want to
change; everything else still resolves from compiled-in defaults. This
mirrors how most developers expect dotfile-style config to behave
(same model as Git config, ripgrep, etc.) and is what makes
customization low-friction rather than "copy the whole default file
and edit it."

### 15.2 Example `keymap.toml`

```toml
# ~/.config/lazycmake/keymap.toml
# Only overrides — unspecified actions keep the built-in vim-like defaults.

[global]
quit = "q"
help = "h"

[main_workspace]
build = "b"
run = "r"
configure = "c"
generate_cmake = "g"

[main_workspace.panels]
next = "tab"
previous = "shift+tab"

# Re-bind build to capital B instead, e.g. to free up 'b' for a plugin action:
# build = "B"
```

### 15.3 Hot reload

`SettingsManager`, `KeymapManager`, and `ThemeManager` each register their
own config directory with the same `FileWatcher` used for project source
files (§12), just pointed at `~/.config/lazycmake/` instead of the
project root. On change, they re-parse and publish a `ConfigReloadedEvent`
that the TUI layer subscribes to — pressing save on a theme file in a
text editor in another terminal pane updates the running app's colors on
the next render tick, no restart needed. (`settings.toml` entries that
affect process-level things like the chosen build backend executable
path are flagged `requiresRestart = true` in the schema and surfaced as
such in the in-app Settings screen rather than silently hot-reloaded.)

### 15.4 Project templates

A template is a directory:

```
~/.config/lazycmake/templates/my-game-template/
├── template.json        # metadata: name, description, default deps, target shape
├── src/main.cpp.tmpl     # Mustache-style {{project_name}} substitution
└── include/.gitkeep
```

`TemplateManager` merges built-in templates (`resources/templates/`,
covering plain Executable/Static/Shared starting points) with user and
plugin-contributed ones for the New Project wizard's "Templates" entry
from the startup screen.

---

## 16. Plugin System

### 16.1 Goals

Make it possible to add a feature (Conan support, Doxygen generation,
clang-tidy integration, CTest panel) **without forking LazyCMake**, using
the exact same extension points (`EventBus`, `IBuildStep`,
`IDependencyProvider`, `IPanelProvider`) that the built-in features
above are themselves implemented with — there is deliberately no
"built-in features get a private API, plugins get a lesser one" split.

### 16.2 Plugin ABI

Plugins are compiled as shared libraries (`.so` / `.dylib` / `.dll`)
exposing a single C-linkage entry point to dodge C++ ABI fragility
across compilers:

```cpp
// include/lazycmake/plugins/plugin_api.hpp

#define LAZYCMAKE_PLUGIN_API_VERSION 1

extern "C" {
    struct PluginDescriptor {
        int          apiVersion;     // must equal LAZYCMAKE_PLUGIN_API_VERSION
        const char*  name;
        const char*  version;
        lazycmake::IPlugin* (*createPlugin)();
        void (*destroyPlugin)(lazycmake::IPlugin*);
    };

    // Every plugin .so/.dll must export exactly this symbol:
    PluginDescriptor lazycmake_plugin_descriptor();
}
```

`PluginHost::loadPlugin(path)` `dlopen`s the library, resolves
`lazycmake_plugin_descriptor`, checks `apiVersion` (refuses to load on
mismatch rather than risking UB from a struct-layout change), then calls
`createPlugin()` and `onLoad(PluginContext&)`.

```cpp
struct IPlugin {
    virtual void onLoad(PluginContext& ctx) = 0;
    virtual void onUnload() = 0;
    virtual ~IPlugin() = default;
};

struct PluginContext {
    PluginEventBus&          eventBus;        // subscribe-only facade, §8
    DependencyRegistry&      dependencies;     // can register() new providers
    PanelRegistry&           panels;           // can register() new IPanelProvider
    BuildStepRegistry&       buildSteps;       // can register() new IBuildStep
    const SettingsManager&   settings;         // read-only
};
```

### 16.3 Manifest-based discovery (so users don't need to know `.so` paths)

```toml
# ~/.config/lazycmake/plugins/clang-tidy.toml
name = "clang-tidy"
enabled = true
library = "clang_tidy_plugin.so"   # resolved relative to the manifest's own dir,
                                     # or an absolute path
```

`PluginHost` scans `~/.config/lazycmake/plugins/*.toml` at startup (and
on hot-reload of that directory, same mechanism as §15.3), loading every
manifest with `enabled = true`. The Settings screen's plugin list lets a
user toggle `enabled` through the UI instead of hand-editing TOML, but
the file remains the source of truth either way — UI and config-file
customization stay two views of one model, consistent with §2.

### 16.4 Example: stretch-goal plugins from the brief

| Plugin | Implements | Behavior |
|---|---|---|
| Conan | `IDependencyProvider` | Queries local Conan cache / conancenter for packages, emits `find_package` CMake fragments |
| vcpkg | `IDependencyProvider` | Already partially built-in (§13); plugin form adds custom registries |
| Doxygen | `IBuildStep` (post-build, optional) | Generates a `docs` custom target, wired into generated CMake via an `IBuildStep::contributeCMake()` hook |
| clang-format | `IBuildStep` (pre-build) + `FileAddedEvent` subscriber | Auto-formats new files on save/detection |
| clang-tidy | `IBuildStep` | Adds `CMAKE_CXX_CLANG_TIDY` wiring, surfaces lint output in the Output panel via the same `DiagnosticParser` (§11) |
| CTest | `IPanelProvider` + `IBuildStep` | Adds a "Tests" panel (4th column or modal) listing `ctest` cases with pass/fail, re-using `RunManager`'s output-streaming machinery |

---

## 17. UI Flow Diagrams

### 17.1 Top-level navigation

```
┌────────────┐  New/Open/Recent/Template   ┌────────────┐  finish wizard   ┌──────────────┐
│  Startup    ├─────────────────────────────►│  Wizard     ├──────────────────►│ MainWorkspace│
│  Screen     │                              │  (5 steps)  │                  │ (Files|Targets│
└─────┬──────┘                              └────────────┘                  │  |Output)     │
      │ Settings                                                            └──────┬───────┘
      ▼                                                                            │ h / esc
┌────────────┐                                                              ┌──────▼───────┐
│  Settings  │◄─────────────────────────────────────────────────────────────┤  Overlays:    │
│  Screen    │                              s (from MainWorkspace too)      │  Help, Build, │
└────────────┘                                                              │  Run, Deps,   │
                                                                             │  Files dialog │
                                                                             └──────────────┘
```

### 17.2 New Project Wizard steps

```
[1] Name & Language ─► [2] Project Type ─► [3] Standard ─► [4] Build System ─► [5] Dependencies ─► Generate & Open
     (C/C++/Both)        (Exe/Static/        (C++17/20/23)   (Ninja/Make)         (checklist,
                          Shared)                                                  FetchContent/
                                                                                    CPM/vcpkg)
```

Each step is its own FTXUI `Component`; `Back`/`Next` are both
button-clickable and keyboard-navigable (`l` / `h` per the brief's
vim-like scheme reused inside the wizard for consistency). On the final
step, "Generate" calls `ModernCMakeGenerator::generate()` and
`ProjectRepository::save()`, then transitions straight into
`MainWorkspace` with the Output panel showing the just-completed
Configure step.

### 17.3 Build → Run flow

```
MainWorkspace ─ b ─► Build overlay (state machine §9.1, live checklist + streaming output)
                          │ on BuildFinishedEvent(success)
                          ▼
                     (stays open, or user presses r)
                          │
                          ▼
                     Run overlay (state machine §9.2: args/env/cwd form ─► live stdout/stderr ─► exit code)
```

---

## 18. Testing Strategy

Matches the brief's "unit-testable core" requirement directly, enabled
by the FTXUI-free Core boundary (§4, §5):

| Layer | Test approach | Example cases |
|---|---|---|
| Core models | Catch2 unit tests, no I/O | `TargetValidator` rejects duplicate target names; rejects a target linking itself; accepts a valid diamond dependency graph |
| CMake Generator | Golden-file tests: `Project` fixture → generated text → compare against checked-in expected `.cmake` fixture | Adding a dependency adds exactly one `target_link_libraries` entry; switching `CppStandard` changes `target_compile_features` only, nothing else byte-diffs |
| Generator + real CMake | Integration test: generate into a temp dir, actually invoke `cmake --version`-gated `cmake -S . -B build` in CI, assert exit code 0 | Catches "generates syntactically invalid CMake" class of bugs that golden-file text comparison alone would miss |
| Build/Run managers | Fake `IBuildBackend`/test double, assert state machine transitions and emitted events, no real subprocess | Cancelling mid-build transitions to `Idle` not `Failed`; backend error transitions to `Failed(stage)` with captured stderr |
| File watcher | Inject fake fs events into `FileClassifier` directly (skip real `efsw` in unit tests); separate slow/manual integration test touches real files | `.gitignore`-style pattern correctly excludes `build/` |
| Config layering | Pure function tests on the merge logic with in-memory TOML strings standing in for each layer | A user override of one keybinding doesn't drop the other 30 defaults |
| Plugin host | Test plugin (trivial `.so` built as a test fixture) loaded/unloaded; version-mismatch descriptor rejected without crashing | |
| TUI | Manual/exploratory + FTXUI's own snapshot-testing support for component rendering, kept intentionally lighter-weight than Core | |

CI matrix: Linux (gcc + clang), macOS (clang), Windows (MSVC), each
running the full Catch2 suite plus the CMake-integration smoke test.

---

## 19. Implementation Roadmap

Phased so that something runnable exists early, and so the
customization surfaces (config layering, plugin ABI) land *before* a
large surface of built-in features is built on top of them — avoiding a
retrofit.

**Phase 0 — Skeleton & tooling (this session)**
Repo layout, root `CMakeLists.txt` with `FetchContent` for all
dependencies, `Project`/`Target`/`DependencySpec` structs + JSON
(de)serialization, Catch2 wired up, CI skeleton, the `ftxui/` grep lint.

**Phase 1 — Core domain, no UI**
`ProjectRepository` (load/save/migrate), `TargetValidator`,
`DependencyRegistry` with built-in presets, full unit test coverage for
all of the above. Deliverable: a CLI smoke-test binary that builds a
`Project` in code and round-trips it through JSON — no TUI yet.

**Phase 2 — CMake Generator**
`CMakeFile`/`CMakeStatement` AST, `CMakePrinter`, `ModernCMakeGenerator`,
golden-file tests, the real-CMake integration test. Deliverable: feed it
a hand-built `Project`, get a `CMakeLists.txt` tree that actually
configures and builds a trivial "hello world" target with real CMake +
Ninja.

**Phase 3 — Configuration system**
`SettingsManager`, `KeymapManager`, `ThemeManager`, the layered-resolution
merge logic, built-in `resources/` defaults baked in. Deliverable: a
config-layering unit test suite proving partial overrides work, before
any TUI consumes these managers.

**Phase 4 — Build & Run engines**
`IBuildBackend`/`NinjaBackend`/`MakeBackend` via `reproc`, `BuildManager`
state machine, `RunManager`, `EventBus`. Deliverable: CLI smoke-test
binary that configures+builds+runs the Phase 2 hello-world project and
prints streamed output, still no TUI.

**Phase 5 — TUI shell**
FTXUI app skeleton, `ScreenStack`, Startup screen, `MainWorkspace`'s
three-panel layout (static data first), `KeymapManager`/`ThemeManager`
wired into real rendering, `HelpOverlay`. Deliverable: navigable,
themeable, rebindable shell with no live build/file functionality yet.

**Phase 6 — Wire Core into TUI**
File browser backed by real `std::filesystem` + `FileWatcher`, Target
manager backed by real `Project`, Build/Run overlays wired to
`BuildManager`/`RunManager` with live streaming, New Project Wizard,
Dependency Manager dialog. Deliverable: feature-complete for the brief's
core (non-stretch) requirements.

**Phase 7 — Plugin system**
`PluginHost`, ABI, manifest discovery, port one real example plugin
(clang-format, simplest) to validate the API end-to-end before
documenting it as stable.

**Phase 8 — Polish & packaging**
Cross-platform CI hardening, install packaging (per-OS), docs, second
example plugin (CTest panel, exercises `IPanelProvider`) to prove the
panel extension point isn't clang-format-shaped only.

**Stretch phases**, only after Phase 8: CMake syntax highlighting,
embedded editor, presets editor, LSP integration, Git panel — each gated
behind its own design note since they're sizeable enough to deserve one
(per the brief, this document intentionally does not attempt to fully
design those now).

---

## 20. Stretch Goals — Design Hooks

These are deliberately *not* designed in depth here, but each has a
clear landing spot in the architecture above, so none of them require
restructuring Core when picked up later:

| Stretch goal | Landing spot |
|---|---|
| CMake syntax highlighting | New `IPanelProvider`-style "file viewer" panel; highlighting itself is a self-contained lexer, no Core changes |
| Embedded editor | Extends the same viewer panel with FTXUI text-input components; interacts with hand-edit detection (§10.4) by writing through the same hashed-write path |
| Presets editor | New screen over `CMakePresets.json`, modeled exactly like `BuildConfig` in §6 — same generator-AST approach extended to preset JSON |
| Compile commands generation | `IBuildBackend` already shells out to CMake/Ninja, both of which support `CMAKE_EXPORT_COMPILE_COMMANDS` — one generator flag, no architecture change |
| LSP integration | Consumes the `compile_commands.json` above; a new optional background process managed similarly to `IBuildBackend`'s subprocess handling |
| Build history | `BuildManager` already emits a full event stream (§8); persisting it is an `EventBus` subscriber that appends to a local SQLite/JSON log, no new core concept |
| Performance profiling | `IBuildStep` post-build hook that times each target's build, surfaced in the existing Output panel |
| Package publishing | New `IBuildStep`/CLI mode wrapping `cpack`, which CMake already provides |
| Project templates | Already first-class, not stretch (§15.4) |
| Git integration | Largest stretch item; would warrant its own ADR (`docs/adr/`) and likely its own panel + `IPanelProvider`, deliberately scoped out of this document |

---

*End of design document. Implementation of Phase 0/1 (Core layer
skeleton) follows in the accompanying source tree under `include/` and
`src/`.*
