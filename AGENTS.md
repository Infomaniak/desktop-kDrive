# kDrive Desktop — Root AGENTS.md

## Project Snapshot
C++20 desktop sync client for Infomaniak kDrive. Single-product monolith built with CMake + Conan 2. Ships two processes: a background **server** daemon (`src/server/`) and a **Qt Widgets GUI** (`src/gui/`). All sync logic lives in `src/libsyncengine/`. Targets macOS, Windows, and Linux.

All code is in the `KDC` namespace. See sub-AGENTS.md files linked below for component-specific guidance.

## Setup & Build
```bash
# Install Conan 2 dependencies (run from repo root)
conan install . --build=missing -s build_type=Debug

# Configure + build (example: macOS)
cmake -B build-macos -DCMAKE_BUILD_TYPE=Debug -DBUILD_UNIT_TESTS=ON
cmake --build build-macos --parallel

# Run a test binary
./build-macos/test/libsyncengine/kDrive_test_syncengine

# Format code (auto-applied by pre-commit hook)
clang-format -i <file>
```

## Universal Conventions
- **Language:** C++20. `#pragma once` for header guards. All code in `KDC` namespace.
- **Style:** Google-based clang-format, 4-space indent, 130-char line limit. Enforced by `.githooks/pre-commit`.
- **Includes:** Relative to `src/` root — e.g., `#include "libcommon/utility/types.h"`.
- **Platform files:** Use suffixes `_mac.mm` / `_win.cpp` / `_linux.cpp` for platform-specific code.
- **Logging:** `LOG_INFO`, `LOG_DEBUG`, `LOG_WARN`, `LOG_ERROR` from log4cplus. Never `std::cout`.
- **Commits:** Conventional commits format (`feat(scope):`, `fix(scope):`, `refactor(scope):`), validated by `.githooks/commit-msg`.
- **Branch naming:** No enforced convention currently.

## Security & Secrets
- Never commit API tokens, passwords, or credentials. Use env vars (`KDRIVE_TEST_CI_API_TOKEN`, etc.).
- Keychain access is managed by `libcommonserver/keychainmanager/`.
- Sentry DSN and signing secrets are injected by CI only.

## AGENTS.md Maintenance
> **Important:** When making significant changes to a directory that contains an AGENTS.md file (new patterns, new architecture, new commands), update that AGENTS.md to reflect the changes. Keep documentation in sync with code.

## User Preferences & Auto-Correction
> **New Norms:** If the user corrects you (e.g., "Don't use X, use Y"), add that rule to the "Local norms" section immediately so you don't make the same mistake again.

### Local Norms
<!-- Add project-specific user corrections here -->

## JIT Index

### Source Libraries
- Common types/utilities: `src/libcommon/` → [see AGENTS.md](src/libcommon/AGENTS.md)
- Server utilities + platform I/O: `src/libcommonserver/` → [see AGENTS.md](src/libcommonserver/AGENTS.md)
- Parameters database: `src/libparms/` → [see AGENTS.md](src/libparms/AGENTS.md)
- Sync engine (core): `src/libsyncengine/` → [see AGENTS.md](src/libsyncengine/AGENTS.md)
- GUI (Qt Widgets): `src/gui/` → [see AGENTS.md](src/gui/AGENTS.md)
- Background server process: `src/server/` → [see AGENTS.md](src/server/AGENTS.md)

### Tests
- Sync engine tests: `test/libsyncengine/` → [see AGENTS.md](test/libsyncengine/AGENTS.md)

### Infrastructure
- Shell extensions (macOS/Windows): `extensions/` → [see AGENTS.md](extensions/AGENTS.md)
- Build scripts + Conan recipes: `infomaniak-build-tools/` → [see AGENTS.md](infomaniak-build-tools/AGENTS.md)

### Quick Find Commands
```bash
# Find a class definition
rg -n "class ClassName" src/

# Find all usages of a type
rg -n "TypeName" src/ -g "*.h" -g "*.cpp"

# Find IPC job for a feature
rg -n "class .*Job" src/server/requests/ src/gui/

# Find platform-specific implementation
rg -ln "platform" src/ -g "*_mac.mm" -g "*_win.cpp"

# Find test for a class
rg -rn "TestClassName" test/
```

## Definition of Done
- Code compiles on all 3 platforms (CI validates).
- `clang-format` reports no diff on touched files.
- New logic has a corresponding test in `test/`.
- No hardcoded credentials or platform-specific paths in shared code.
- CI passes: `kdrive-desktop-ci.yml`.
