# gui4/linux — Linux v4 Qt/QML Frontend

## Maintenance Cadence

- This area changes frequently and often receives large feature additions.
- Update this file very regularly; it must stay aligned with the current architecture and workflows.
- Any significant change under `src/gui4/linux/` (new layer, new pattern, new build/run command) must include an
  `AGENTS.md` update in the same PR.

## User Preferences & Auto-Correction

> New norm: if the user corrects a working rule, add it here immediately to avoid repeating the same mistake.

### Local Norms (Linux v4)

- In versioned documentation, use repo-relative paths (no hardcoded absolute paths).
- Do not add links to `.md` files that are not versioned in git.
- For Linux validation, prefer `infomaniak-build-tools/linux/build-release-via-podman.sh`.
- Prefer documenting private implementation helpers in `.cpp` rather than headers.
- Do not introduce raw `int` in new code when a fixed-width type fits (`uint8_t`, `int32_t`, ...).
- Do not run `clang-format` on `CMakeLists.txt` in this repository.
- For any branch named `linux-v4/*`: create it from `linux-v4/main`, and compute diffs against `linux-v4/main` by
  default. `linux-v4/main` is the Linux v4 integration branch regularly rebased on `develop`.

## Scope

- Linux-only v4 frontend based on Qt 6.8 (QML + C++).
- This package handles UI bootstrap and server communication only.
- Sync business logic stays in `src/libsyncengine/` and server-side jobs.

## Current Structure

- `main.cpp`: process entry point + single-instance lock file.
- `appclientlinux.*`: top-level app wiring (logging, IPC lifecycle, dispatcher/service ownership,
  QML engine bootstrap, and context-property injection for cache/services).
- `communicationlayer/ipcclient.*`: raw TCP JSON transport, request/reply correlation, reconnect-before-first-connect
  logic.
- `communicationlayer/signaldispatcher.*`: server-push signal fanout to registered handlers.
- `app/services/commservice.*`: typed request/signal facade above `IpcClient`.
- `app/cache/appcache.*`: central observable cache (`AppCache` QObject) — typed snapshots and incremental
  `upsert*`/`remove*` slots for users, accounts, drives, available drives, syncs, and errors; QML models and
  services read from it instead of parsing transport payloads directly.
- `app/cache/cachepipeline.*`: single wiring point from `CommService` server-push signals to `AppCache`
  incremental updates (`signal -> cache` pipeline).
- `app/models/*listmodel.*`: QML-facing `QAbstractListModel` adapters fed from `AppCache` (`rowCount/data/roleNames`
  contract for views).
- `ui/`: QML shell and design tokens.

## Build And Validation

```bash
# From repo root: configure a Linux debug tree (example used in CLion workflow)
cmake --preset conan-debug -S . -B build-linux/build/build/Debug

# Preferred Linux validation build
infomaniak-build-tools/linux/build-release-via-podman.sh

# Optional fast local iteration build (not the canonical validation path)
cmake --build build-linux/build/build/Debug --target kdrive_qml -- -j 12
```

## Architecture Rules

- Keep layers strict:
    - `communicationlayer/*` = transport/protocol mechanics only.
    - `app/services/*` = typed backend facade for upper layers.
    - `ui/*` = presentation and bindings; no protocol code.
- QML must never call `IpcClient` directly.
- New backend calls must be added in `CommService`, not in UI files.
- Server-push events must be registered in `CommService::register*Handlers` and re-emitted as typed Qt signals.
- Connections from `CommService` signals to `AppCache` slots must live in `CachePipeline` only
  (no duplicated service-level wiring).
- Cache removals must preserve graph coherence in flat snapshots (for example account/drive removal should cascade
  dependent drives/syncs/errors entries).
- App bootstrap should inject C++ facades (`appCache`, `userService`, `driveService`, `syncService`,
  `driveListModel`, `syncListModel`) at the QML engine boundary (`QQmlApplicationEngine::rootContext()`).
- QML list views should consume `QAbstractListModel` adapters from `app/models/` rather than reading transport DTOs
  directly.
- When object lifetime is uncertain in async callbacks, guard with `QPointer<T>`.

## IPC And Error Handling

- Transport is loopback TCP + JSON envelope shared with `libcommon/comm.h` / `libcommon/commjson.h`.
- `IpcClient` treats post-connection socket failure as fatal (disconnect/error after first successful connection exits
  process).
- Request methods should parse `Poco::DynamicStruct` into typed DTOs before exposing data upward.
- Use shared `msgParam*` keys and enums from `libcommon`; avoid ad-hoc string keys.

## Coding Conventions (Linux v4)

- Use `QLoggingCategory` + `qCDebug/qCInfo/qCWarning/qCCritical`; no `std::cout`.
- Do not introduce raw `int` when fixed-width types are appropriate (`int32_t`, `uint8_t`, ...).
- Prefer documenting private implementation helpers in `.cpp` rather than headers.
- Do not run `clang-format` on `CMakeLists.txt` in this repository.

## Change Playbooks

### Add a new request in `CommService`

1. Verify `RequestNum` and `msgParam*` exist in `libcommon`.
2. Add callback typedef if needed.
3. Add method declaration in `commservice.h`.
4. Implement method in `commservice.cpp` using `_ipcClient.sendRequest(...)`.
5. Convert IPC payload to typed result object before invoking callback.

### Add a new server-push signal

1. Verify `SignalNum` and payload keys in `libcommon`.
2. Register handler in the appropriate `register*Handlers` method.
3. Parse payload safely into typed fields/DTOs.
4. Emit a typed Qt signal from `CommService`.

## Quick Find

```bash
# Linux v4 files
find src/gui4/linux -type f | sort

# IPC request methods and callsites
rg -n "request[A-Z].*\\(" src/gui4/linux/app/services/commservice.* -g "*.h" -g "*.cpp"

# Signal registrations
rg -n "registerHandler\\(" src/gui4/linux/app/services/commservice.cpp

# Shared request/signal enums and JSON keys
rg -n "enum class (RequestNum|SignalNum)|msgParam" src/libcommon/comm.h src/libcommon/commjson.h

# Logging categories in Linux v4 frontend
rg -n "Q_LOGGING_CATEGORY|qC(Debug|Info|Warning|Critical)" src/gui4/linux -g "*.cpp" -g "*.h"
```

## Pre-PR Checks

```bash
# Build the Linux v4 frontend target locally
cmake --build build-linux/build/build/Debug --target kdrive_qml -- -j 12

# Run the canonical Linux validation build
infomaniak-build-tools/linux/build-release-via-podman.sh
```
