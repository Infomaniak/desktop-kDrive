# kDrive Desktop — Root AGENTS.md

## Project Snapshot
C++20 desktop sync client for Infomaniak kDrive. Single-product monolith built with CMake + Conan 2. Ships two processes: a background **server** daemon (`src/server/`) and a **Qt Widgets GUI** (`src/gui/`). All sync logic lives in `src/libsyncengine/`. Targets macOS, Windows, and Linux.

All code is in the `KDC` namespace.

## How to Use These Files
At the start of every session:
1. **Read this file** — universal conventions, security rules, and the component index below.
2. **Read the sub-AGENTS.md for each component you'll touch** — e.g. editing `src/libsyncengine/` → read [`src/libsyncengine/AGENTS.md`](src/libsyncengine/AGENTS.md) before writing any code.

Nearest file wins: the sub-AGENTS.md closest to the file you're editing takes precedence over this root file.

## Setup & Build
```bash
# Install Conan 2 dependencies (run from repo root)
conan install . --build=missing -s build_type=Debug

# Configure + build (example: macOS)
cmake -B build-macos -DCMAKE_BUILD_TYPE=Debug -DBUILD_UNIT_TESTS=ON
cmake --build build-macos --parallel

# Run a test binary
./build-macos/bin/kDrive_test_syncengine

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
- In versioned documentation such as `AGENTS.md`, use repo-relative paths, not hardcoded absolute filesystem paths.
- For Linux builds/validation, use `infomaniak-build-tools/linux/build-release-via-podman.sh` rather than direct `cmake --build`.
<!-- Add project-specific user corrections here -->
- Prefer documentation for private implementation methods in the `.cpp` file rather than the header.
- Do not introduce raw `int` in new code when a named fixed-width type fits the use case (e.g. `uint8_t`, `int32_t`, ...).

## JIT Index

### Source Libraries
- Common types/utilities: `src/libcommon/` → [see AGENTS.md](src/libcommon/AGENTS.md)
- Server utilities + platform I/O: `src/libcommonserver/` → [see AGENTS.md](src/libcommonserver/AGENTS.md)
- Parameters database: `src/libparms/` → [see AGENTS.md](src/libparms/AGENTS.md)
- Sync engine (core): `src/libsyncengine/` → [see AGENTS.md](src/libsyncengine/AGENTS.md)
- GUI (Qt Widgets, legacy): `src/gui/` → [see AGENTS.md](src/gui/AGENTS.md)
- GUI (macOS Swift redesign for v4): `src/front/macOS/` → [see AGENTS.md](src/front/macOS/AGENTS.md)
- GUI (Windows WinUI3 redesign for v4): `src/gui4/` → [see AGENTS.md](src/gui4/AGENTS.md)
- Background server process: `src/server/` → [see AGENTS.md](src/server/AGENTS.md)

### Tests
- Test infrastructure overview: `test/` → [see AGENTS.md](test/AGENTS.md)
- Sync engine tests: `test/libsyncengine/` → [see AGENTS.md](test/libsyncengine/AGENTS.md)

### Infrastructure
- Shell extensions (macOS/Windows): `extensions/` → [see AGENTS.md](extensions/AGENTS.md)
- Conan dependencies & recipes: `infomaniak-build-tools/conan/` → [see AGENTS.md](infomaniak-build-tools/conan/AGENTS.md)

### Legal & Licensing
- Third-party licenses and attributions: [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)
- Project license: [`LICENSE`](LICENSE) (GPL v3)

### Quick Find Commands
```bash
# Find a class definition
rg -n "class ClassName" src/

# Find all usages of a type
rg -n "TypeName" src/ -g "*.h" -g "*.cpp"

# Find IPC job for a feature
rg -n "class .*Job" src/server/comm/guijobs/ src/gui/ -g "*.h"

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
- New dependencies licenses are documented in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
- CI passes: `kdrive-desktop-ci.yml`.
