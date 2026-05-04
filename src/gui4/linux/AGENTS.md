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
- On a Linux host, validate natively: run `./infomaniak-build-tools/conan/build_dependencies.sh Debug`, configure with
  the generated Conan/CMake Debug preset, then build `kDrive`,
  `kDrive_client`, and `kdrive_qml`. Do not use the Podman release script for this local Linux validation path.
- Prefer documenting private implementation helpers in `.cpp` rather than headers.
- Do not introduce raw `int` in new code when a fixed-width type fits (`uint8_t`, `int32_t`, ...).
- Do not run `clang-format` on `CMakeLists.txt` in this repository.
- For shared infrastructure classes, document the class role explicitly in the header comment (and non-role when
  relevant).
- In range-for loops over associative containers, prefer `std::views::keys` / `std::views::values` over structured
  bindings with an unused `_` element when only keys or only values are needed.

## Scope

- Linux-only v4 frontend based on Qt 6.8 (QML + C++).
- This package handles UI bootstrap and server communication only.
- Sync business logic stays in `src/libsyncengine/` and server-side jobs.

## Current Structure

- `main.cpp`: process entry point + single-instance lock file.
- `appclientlinux.*`: top-level app wiring (logging, IPC lifecycle, dispatcher/service ownership).
- `communicationlayer/ipcclient.*`: raw TCP JSON transport, request/reply correlation, reconnect-before-first-connect
  logic.
- `communicationlayer/signaldispatcher.*`: server-push signal fanout to registered handlers.
- `app/services/commservice.*`: typed request/signal facade above `IpcClient`.
- `app/services/serviceactiontracker.*`: shared persistent state for in-flight service actions
  (`begin/end/isActionPending/isServicePending`).
- `app/services/serviceeventbus.*`: shared high-level service event hub (single UI subscription point for generic
  cross-service failures). Owned once by `AppClientLinux` and injected by reference into app services.
- `app/cache/appcache.*`: durable graph-backed cache (`AppCache` QObject) — owns configured users/accounts/drives/syncs,
  split sync/server errors, per-user available drives, cascade removals, and derived read models.
- `app/cache/cachepipeline.*`: unique bridge for `CommService -> AppCache` push signals.
- `app/cache/cachetypes.h`: cache read models and onboarding keys (`SyncContext`, `DriveContext`,
  `AvailableDriveContext`, `AvailableDriveKey`, `PendingSyncConfig`).
- `app/cache/mainselectionstore.*`: sync-first main-shell selection owner (`currentSyncDbId`) and selection healing.
    - emits `currentSyncContextChanged()` as a coarse invalidation signal when the current sync context stays selected
      but the underlying cache graph changes.
- `app/cache/onboardingstate.*`: onboarding-only selected user, selected available-drive keys, and pending sync configs.
- `app/services/cachehydrator.*`: sequential initial snapshot loader for users, accounts, drives, syncs, and sync
  errors.
- `app/services/driveservice.*`: drive use-case facade driven by `ServiceActionTracker` + `ServiceEventBus`.
- `app/services/syncservice.*`: sync use-case facade driven by `ServiceActionTracker` + `ServiceEventBus`.
- `ui/`: QML shell and design tokens.

## Build And Validation

```bash
# From repo root: install Debug dependencies and generate Conan/CMake presets
./infomaniak-build-tools/conan/build_dependencies.sh Debug

# Configure the generated Debug preset, then build the relevant Linux targets
cmake --preset conan-debug -S . -B build-linux/build/build/Debug
cmake --build build-linux/build/build/Debug --target kDrive kdrive_qml -- -j 8
```

## Architecture Rules

- Keep layers strict:
    - `communicationlayer/*` = transport/protocol mechanics only.
    - `app/services/*` = typed backend facade for upper layers.
    - `ui/*` = presentation and bindings; no protocol code.
- QML must never call `IpcClient` directly.
- New backend calls must be added in `CommService`, not in UI files.
- Server-push events must be registered in `CommService::register*Handlers` and re-emitted as typed Qt signals.
- When object lifetime is uncertain in async callbacks, guard with `QPointer<T>`.
- For cross-service UI error notification, use a single shared `ServiceEventBus` instance and inject it by reference
  into each service (`UserService`, `DriveService`, `SyncService`, ...).
- Keep responsibilities split:
  `ServiceEventBus` for transient events, `ServiceActionTracker` for durable pending-action state.
- Do not create per-service formatted error-string state for UI display; services emit generic bus signals and keep
  structured backend error information in request handlers/logs.
- `DriveService` and `SyncService` use `ServiceActionTracker` for loading/pending state and `ServiceEventBus` for
  transient failure notification; avoid reintroducing local `lastError` / ad hoc pending counters there.
- `AppCache`, `MainSelectionStore`, and `OnboardingState` mutations must run on the Qt main thread.
- `AppCache` must not own mutable main selection; derive main context through `MainSelectionStore.currentSyncDbId`.
- Store available drives per user via `AppCache::replaceAvailableDrivesForUser(...)`; do not reintroduce a global
  available-drives snapshot.
- `CachePipeline` owns the direct push-signal bridge from `CommService` to `AppCache`; service classes should not wire
  those pushes themselves.

## IPC And Error Handling

- Transport is loopback TCP + JSON envelope shared with `libcommon/comm.h` / `libcommon/commjson.h`.
- `IpcClient` treats post-connection socket failure as fatal (disconnect/error after first successful connection exits
  process).
- Request methods should parse `Poco::DynamicStruct` into typed DTOs before exposing data upward.
- Generic UI-facing request failures from high-level services should be emitted through `ServiceEventBus` so UI can
  subscribe once (`genericErrorOccurred()`).
- Action-level and per-service loading state should come from `ServiceActionTracker`
  (`isActionPending(...)`, `isServicePending(...)`).
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
# Native Linux validation from repo root
./infomaniak-build-tools/conan/build_dependencies.sh Debug
cmake --preset conan-debug -S . -B build-linux/build/build/Debug
cmake --build build-linux/build/build/Debug --target kDrive kDrive_client kdrive_qml -- -j 8
```
