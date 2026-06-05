# gui4/linux — Linux v4 Qt/QML Frontend

## Maintenance Cadence

- This area changes frequently and often receives large feature additions.
- Update this file very regularly; it must stay aligned with the current architecture and workflows.
- Any significant change under `src/gui4/linux/` (new layer, new pattern, new build/run command) must include an
  `AGENTS.md` update in the same PR.

## User Preferences & Auto-Correction

> New norm: if the user corrects a working rule, add it here immediately to avoid repeating the same mistake.

### Local Norms (Linux v4)

- In versioned documentation, use repo-relative paths, not hardcoded absolute paths.
- Do not add links to `.md` files that are not versioned in git.
- On a Linux host, validate natively: run `./infomaniak-build-tools/conan/build_dependencies.sh Debug`, configure with
  the generated Conan/CMake Debug preset, then build `kDrive`, `kDrive_client`, and `kdrive_qml`. Do not use the Podman
  release script for this local Linux validation path.
- Prefer documenting private implementation helpers in `.cpp` rather than headers.
- Do not introduce raw `int` in new code when a fixed-width type fits (`uint8_t`, `int32_t`, ...).
- Do not run `clang-format` on `CMakeLists.txt` in this repository.
- For shared infrastructure classes, document the class role explicitly in the header comment when relevant.
- In range-for loops over associative containers, prefer `std::views::keys` / `std::views::values` over structured
  bindings with an unused `_` element when only keys or only values are needed.
- For Linux v4 model/UI checks, build only the `kdrive_qml` target unless a broader backend/server validation is
  explicitly needed.
- For tray fallback testing, `KDRIVE_FORCE_NO_TRAY=1` is Debug-only and forces the startup tray probe to stay disabled.
- Avoid magic layout values in QML; put reusable or semantic dimensions and ratios in `ui/tokens/` with explicit names.
- Store Linux v4 app-level non-translatable constants in `app/appconstants.h`; keep it header-only while constants stay
  simple.
- Keep simple onboarding external-link actions in `OnboardingFlowController` when they do not mutate app/backend state;
  put multi-service onboarding backend effects in a dedicated onboarding coordinator.
- Keep `AppClientLinux` as an application composition root. Move multi-step feature workflows such as onboarding login into
  dedicated coordinators instead of accumulating workflow lambdas in `AppClientLinux`.
- `Qt.labs.lottieqt` renders Lottie JSON assets, not `.lottie` zip containers.
- For Qt 6.11+ onboarding Lottie assets, prefer generated QML from `lottietoqml` over PNG spritesheets when the
  generated output builds and visually matches the source animation.
- `lottietoqml` expects the JSON animation payload; extract `animations/<id>.json` from `.lottie` containers before
  generating QML components.
- Version generated onboarding animation QML files in `ui/onboarding/animations/`, but do not edit them manually.
  Regenerate them from the source `.lottie` asset and keep the "Do not edit" header.

## Scope

- Linux-only v4 frontend based on Qt 6.8 (QML + C++).
- This package handles UI bootstrap and server communication only.
- Sync business logic stays in `src/libsyncengine/` and server-side jobs.

## Current Structure

- `main.cpp`: process entry point + single-instance lock file.
- `appclientlinux.*`: top-level app wiring (logging, IPC lifecycle, dispatcher/service/coordinator ownership).
- `app/appconstants.h`: app-level non-translatable constants, mirroring the Windows `AppConstants` role where useful.
- `app/systraycontroller.*`: Linux system tray ownership, 5-state tray icon selection, GNOME-compatible tray menu
  actions, fallback-to-window startup behavior, retry loop for late tray availability, and main QML window show/hide
  behavior.
- `communicationlayer/ipcclient.*`: raw TCP JSON transport, request/reply correlation, reconnect-before-first-connect
  logic.
- `communicationlayer/signaldispatcher.*`: server-push signal fanout to registered handlers.
- `app/services/commservice.*`: typed request/signal facade above `IpcClient`.
- `app/services/serviceactiontracker.*`: shared persistent state for in-flight service actions
  (`begin/end/isActionPending/isServicePending`).
- `app/services/serviceeventbus.*`: shared high-level service event hub (single UI subscription point for generic
  cross-service failures). Owned once by `AppClientLinux` and injected by reference into app services.
- `app/services/sentryservice.*`: Linux v4 Sentry coordinator. Owns cached consent reconciliation, delayed
  linux-v4-specific Sentry initialization, authenticated user binding, and UI/process capture helpers.
- `app/cache/appcache.*`: durable graph-backed cache (`AppCache` QObject) - owns configured users/accounts/drives/syncs,
  split sync/server errors, per-user available drives, cascade removals, and derived read models.
- `app/cache/cachepipeline.*`: unique bridge for `CommService -> AppCache` push signals.
    - Drops push mutations before `CachePopulator::bootstrapCompleted()` and logs the invariant violation.
- `app/cache/cachetypes.h`: cache read models and onboarding keys (`SyncContext`, `DriveContext`,
  `AvailableDriveContext`, `AvailableDriveKey`, `PendingSyncConfig`).
- `app/cache/mainselectionstore.*`: sync-first main-shell selection owner (`currentSyncDbId`) and selection healing.
    - Emits `currentSyncContextChanged()` as a coarse invalidation signal when the current sync context stays selected
      but the underlying cache graph changes.
- `app/cache/onboardingstate.*`: onboarding-only selected user, selected available-drive keys, and pending sync configs.
- `app/onboarding/onboardingflowcontroller.*`: QML-facing onboarding flow controller aligned with the macOS flow
  (`login -> drive selection -> synchronization -> ready`, with macOS permission steps omitted on Linux). It owns simple
  onboarding UI actions such as opening the account signup URL; OAuth launch, `LOGIN_REQUESTTOKEN`, available-drive
  loading, and sync creation stay outside QML-facing flow state.
- `app/onboarding/onboardinglogincoordinator.*`: login workflow coordinator for onboarding. It wires the flow controller,
  OAuth service, comm service, user service, app cache, and onboarding state so `AppClientLinux` does not accumulate
  login-specific workflow logic.
- `app/onboarding/oauthloginservice.*`: Linux v4 OAuth browser-launch service. It owns PKCE/state generation, idempotent
  browser relaunch during an active authorization, callback validation, and emits the authorization code to app wiring.
  Do not expose OAuth details to QML.
- `app/services/cachepopulator.*`: sequential initial snapshot loader for users, accounts, drives, syncs, and sync
  errors; after bootstrap, activates the server live-info refresh so only drive updates reach `CachePipeline`.
- `app/services/driveservice.*`: targeted drive use-case facade driven by `ServiceActionTracker` + `ServiceEventBus`;
  durable cache mutations stay signal-driven through `CachePipeline`.
- `app/services/syncservice.*`: targeted sync use-case facade driven by `ServiceActionTracker` + `ServiceEventBus`;
  durable cache mutations stay signal-driven through `CachePipeline`.
- `ui/`: QML shell, onboarding screens, design tokens, and bundled UI assets such as tray icons and onboarding Lottie
  animations.
    - `ui/onboarding/animations/`: versioned generated QML animation components produced from Lottie JSON payloads.
      Do not edit these files manually.

### Regenerate Onboarding Lottie QML

Run `lottietoqml` from the Qt/Conan package that provides `qtlottie`. The `.lottie` files are zip containers, so extract
the internal JSON payload first:

```bash
source build-linux/build/build/Debug/generators/conanrun.sh
mkdir -p build-linux/lottie-json

LOTTIE_QML_LICENSE=$(cat <<'EOF'
Infomaniak kDrive - Desktop
Copyright (C) 2023-2026 Infomaniak Network SA

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
EOF
)

unzip -p src/gui4/linux/ui/assets/onboarding/lotties/light/loader-stroke.lottie \
  animations/6160a4f3-bd74-4732-8c38-2a2460359741.json \
  > build-linux/lottie-json/6160a4f3-bd74-4732-8c38-2a2460359741.json
lottietoqml -c -p \
  --copyright-statement \
  "$LOTTIE_QML_LICENSE
Generated by lottietoqml from ui/assets/onboarding/lotties/light/loader-stroke.lottie. Do not edit manually." \
  build-linux/lottie-json/6160a4f3-bd74-4732-8c38-2a2460359741.json \
  src/gui4/linux/ui/onboarding/animations/LoaderStrokeLightAnimation.qml

unzip -p src/gui4/linux/ui/assets/onboarding/lotties/dark/loader-stroke.lottie \
  animations/16dfd798-bc1f-49c0-bfa4-f2849325edc3.json \
  > build-linux/lottie-json/16dfd798-bc1f-49c0-bfa4-f2849325edc3.json
lottietoqml -c -p \
  --copyright-statement \
  "$LOTTIE_QML_LICENSE
Generated by lottietoqml from ui/assets/onboarding/lotties/dark/loader-stroke.lottie. Do not edit manually." \
  build-linux/lottie-json/16dfd798-bc1f-49c0-bfa4-f2849325edc3.json \
  src/gui4/linux/ui/onboarding/animations/LoaderStrokeDarkAnimation.qml
```

## Build And Validation

```bash
# From repo root: install Debug dependencies and generate Conan / CMake presets
./infomaniak-build-tools/conan/build_dependencies.sh Debug

# Configure the generated Debug preset, then build the relevant Linux targets
cmake --preset conan-debug -S . -B build-linux/build/build/Debug
cmake --build build-linux/build/build/Debug --target kDrive kDrive_client kdrive_qml -- -j 8
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
- `CachePipeline` must not let server pushes mutate `AppCache` before the initial `CachePopulator` snapshot has
  completed.
- Full graph snapshots (`USER_INFOLIST`, `ACCOUNT_INFOLIST`, `DRIVE_INFOLIST`, `SYNC_INFOLIST`, initial error list)
  belong to `CachePopulator` bootstrap/reconnect only. Do not expose user/drive/sync full-refresh methods to QML
  services.
- QML-facing services should provide targeted actions only; user/account/drive/sync cache consistency is
  push-signal-driven.
- Onboarding navigation belongs in `OnboardingFlowController`; keep long-running backend work in service facades and
  durable selections in `OnboardingState`. The login screen must not advance optimistically: it advances only after the
  server login-token request succeeds, the logged-in user appears in `AppCache`, and available-drive loading has been
  requested.

## IPC And Error Handling

- Transport is loopback TCP + JSON envelope shared with `libcommon/comm.h` / `libcommon/commjson.h`.
- `IpcClient` treats post-connection socket failure as fatal (disconnect/error after first successful connection exits
  process).
- Request methods should parse `Poco::DynamicStruct` into typed DTOs before exposing data upward.
- Generic UI-facing request failures from high-level services should be emitted through `ServiceEventBus` so UI can
  subscribe once (`genericErrorOccurred()`).
- Sentry captures in Linux v4 should go through `SentryService`; capture helpers must tolerate Sentry being uninitialized
  because consent can disable the native SDK.
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
