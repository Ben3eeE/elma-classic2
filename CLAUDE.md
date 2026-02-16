# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Modernized C++20 port of the original Elasto Mania game (2000). This is a physics-based motorbike game with dual rendering backends (software 8-bit palette and OpenGL), a level editor, and a replay system. Cross-platform via SDL2 (Windows, macOS, Linux).

## Build Commands

```bash
# Setup (one time)
meson setup build                          # debug build
meson setup build -Dbuildtype=release      # release build

# Build
meson compile -C build

# Run (requires game assets copied to build/)
cd build && ./elma

# Clean / full rebuild
meson compile -C build --clean
meson setup --wipe build

# Code formatting (clang-format-19)
ninja -C build clang-format
ninja -C build clang-format-check          # CI check (no modification)

# Static analysis (clang-tidy-19)
ninja -C build clang-tidy

# Configure options
meson configure build -Db_sanitize=address,undefined
meson configure build -Dprofile_performance=true
```

Windows: use `Developer Command Prompt for VS 2022` and add `--vsenv` to setup.

## CI

GitHub Actions runs clang-format-check, clang-tidy (errors only fail), and builds on Ubuntu/Windows/macOS (Intel+ARM). Markdown and docs changes skip CI.

## Architecture

All source files live in the project root. No namespace usage — global-scope functions. No test framework.

### Core Subsystems

- **Physics** (`physics_init`, `physics_move`, `physics_collision`, `ball_collision`, `segments`) — rigid body bike simulation with wheel collision on segment grid
- **Rendering** — software 8-bit palette renderer (`pic8`, `KIRAJ320`, `D_PIC`) and OpenGL renderer (`gl_renderer`). LGR files provide level graphics.
- **Game loop** — `main.cpp` → `platform_init()` → `menu_intro()`. State managed via global `State` pointer.
- **Menu system** (`menu_nav`, `menu_main`, `menu_play`, `menu_options`, `menu_controls`) — `menu_nav` is the modern navigation framework using vectors and callbacks; legacy menus are being migrated to it.
- **Level system** (`level`, `level_load`, `polygon`, `object`, `sprite`) — custom binary format with polygons, objects, and sprites.
- **Replay** (`recorder`) — frame-based recording/playback of player actions.
- **Editor** (`EDITUJ`, `EDITTOLT`, `EDITTOOL`, `editor_canvas`, `editor_topology`, `editor_dialog`) — polygon/object editing with topology validation.
- **State/Settings** (`state`, `eol_settings`) — player profiles, high scores, encrypted state.dat. Settings use typed templates with bounds checking.
- **Platform** (`platform_sdl`, `platform_impl`, `platform_utils`) — SDL2 abstraction for windowing, input, events.
- **Audio** (`sound_engine`, `wav`) — WAV playback with motor frequency modulation.

### File Naming Convention

- **UPPERCASE.CPP/.H** — legacy Hungarian-origin files not yet fully translated/refactored (e.g., `EDITTOLT.CPP`, `LEJATSZO.CPP`)
- **lowercase.cpp/.h** — new or refactored modern C++ files

New code should use lowercase filenames.

### Key Globals

- `State` — player data, high scores, settings
- `EolSettings` — game configuration
- `Motor1`/`Motor2` — bike physics state (multiplayer)
- `Segments` — collision grid

## Code Style

- clang-format enforced: LLVM-based, 4-space indent, 100-column limit, no tabs, left-aligned pointers, braces always inserted
- Commits use conventional format: `type(scope): description` (e.g., `fix(menu_nav):`, `refactor(editplay):`, `feat(menu_nav):`, `translate(...):`). Common types: fix, feat, refactor, chore, translate. Keep commit messages concise — subject line only in most cases, body only when the change is non-obvious. No `Co-Authored-By` trailers.
- Constants: `SCREAMING_SNAKE_CASE`
- Global variables: `PascalCase`
- Functions/local variables: `snake_case`
- Struct/class member variables in templates: trailing underscore (`name_`, `screen_width_`)

## Dependencies

Managed via Meson subprojects (`subprojects/*.wrap`):
- SDL2 2.32.8
- nlohmann_json 3.12.0
- OpenGL 4.1+ (system)
- GLAD (vendored in `include/glad/`)
