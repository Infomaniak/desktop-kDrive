# libcommon — Foundation Library

## Package Identity
Foundational shared library. **Everything depends on it** (`libcommonserver` → `libparms` → `libsyncengine` → `server`; `libcommongui` → `gui`). Defines all core enums, type aliases, IPC protocol, DTO classes, error propagation types, logging helpers, Sentry integration, and the app theme singleton. No platform-specific code except `utility_mac.mm` / `utility_win.cpp` / `utility_linux.cpp`. Export macro: `COMMON_EXPORT`.

## Key Files

| File | What it owns |
|---|---|
| `utility/cstypes.h` | All primitive enums — **no Qt/Poco dependency** |
| `utility/types.h` | Type aliases, `ExitInfo`, concepts, bitwise ops, string conversion macros |
| `comm.h` | Full IPC protocol: `RequestNum`, `SignalNum`, timeout constants |
| `info/*.h` | All DTO classes (UserInfo, DriveInfo, SyncInfo, ErrorInfo, …) |
| `utility/utility.h` | `CommonUtility` — static helpers (paths, strings, UUID, disk space, URL, …) |
| `utility/urlhelper.h` | `UrlHelper` — all Infomaniak API base URLs |
| `log/customlogstreams.h` | `CustomLogStream` / `CustomLogWStream` — typed log stream wrappers |
| `log/sentry/handler.h` | `sentry::Handler` singleton — init, captureMessage, PTrace management |
| `log/sentry/ptraces.h` | All concrete performance trace types mirroring the sync pipeline |
| `theme/theme.h` | `Theme` Qt singleton — icons, branding, systray state |

## Enum Conventions
Every enum lives in `namespace KDC`, in `cstypes.h` or `types.h`.

Rules that **must** be followed:
- Always add `EnumEnd` as the last sentinel — used for iteration and bounds checking.
- Add a `toString(EnumType)` overload declared in `types.h`, defined in `utility/utility.cpp`.
- Use `toStringWithCode(e)` in log macros — produces `"Name(42)"` format.

Key enums to know:
- `ExitCode` / `ExitCause` — returned by every fallible operation (see `ExitInfo` below)
- `SyncStep` — mirrors the full pipeline stages (UpdateDetection1/2, Reconciliation1–4, Propagation1/2)
- `OperationType` / `InconsistencyType` — bitmask enums; bitwise ops enabled via `AllowBitWiseOpEnum` concept
- `NodeType`, `SyncFileStatus`, `SyncDirection`, `VirtualFileMode`, `ReplicaSide`

## Error Propagation: `ExitInfo`
`ExitInfo` (in `types.h`) is the **universal return type** for fallible operations across the whole app.

```cpp
ExitInfo doSomething() {
    if (failed) return {ExitCode::SystemError, ExitCause::FileAccessError};
    return ExitCode::Ok;
}

// Caller:
if (const auto exitInfo = doSomething(); !exitInfo) {
    LOG_WARN(...);
    return exitInfo;
}
```

- `operator bool()` → true if `ExitCode::Ok` or `ExitCode::TokenRefreshed`
- `merge(other)` → keeps highest-priority error from two `ExitInfo` values
- Carries `SourceLocation` for traceability — use `currentLoc()` macro

## DTO Classes (`info/`)
All info classes are plain-data DTOs. Pattern is identical across all of them:
- Private fields with `_camelCase` prefix
- Inline getters/setters
- `toDynamicStruct()` / `fromDynamicStruct()` — Poco JSON for IPC serialization
- `QDataStream >>` / `<<` operators — Qt IPC serialization
- `operator==`

Reference: `src/libcommon/info/userinfo.h`

Key info classes:
- `UserInfo` → `AccountInfo` → `DriveInfo` → `SyncInfo` (ownership chain)
- `ErrorInfo` — full error record with all context fields
- `SyncFileItemInfo` — live sync progress event
- `ParametersInfo` + `ProxyConfigInfo` — all app settings

**Adding a new entity:** copy the pattern from `userinfo.h`. Register both `toDynamicStruct` and the `QDataStream` operators.

## IPC Protocol (`comm.h`)
Defines every request/signal between GUI and server:
- `RequestNum` — ~70 RPC calls, grouped by domain (`LOGIN_*`, `DRIVE_*`, `SYNC_*`, `NODE_*`, …)
- `SignalNum` — ~30 async server→client notifications
- Timeout constants: `COMM_SHORT_TIMEOUT` (1s), `COMM_AVERAGE_TIMEOUT` (10s), `COMM_LONG_TIMEOUT` (60s)

**Adding a new IPC call:** add to `RequestNum` here, then handle symmetrically in `src/server/comm/guicommserver.cpp` and `src/libcommongui/commclient.cpp`.

## Type Aliases to Know
```cpp
using SyncPath    = std::filesystem::path;
using SyncName    = std::filesystem::path::string_type; // std::string (Unix) | std::wstring (Windows)
using NodeId      = std::string;   // server-assigned, stable across renames
using DbNodeId    = int64_t;       // internal SQLite ROWID
using SyncTime    = int64_t;
// ExitInfo is a struct{ExitCode, ExitCause, SourceLocation} — see "Error Propagation" section above
```

Platform string helpers — always use these, never raw casts:
- `SyncName2QStr(n)` / `QStr2SyncName(s)`
- `Path2QStr(p)` / `QStr2Path(s)`
- `Str2SyncName(s)` / `SyncName2Str(n)`

## Sentry Integration
- Initialize once at startup: `sentry::Handler::instance()->init(AppType::Server, breadCrumbsSize)`
- Capture an event: `sentry::Handler::instance()->captureMessage(sentry::Level::Warning, title, msg, user)`
- Rate-limited: max 10 identical events per 10 min, then auto-escalated to Error.
- Performance traces: use concrete types from `log/sentry/ptraces.h`. Scoped traces (RAII) live in `pTraces::scoped::*`; counter-based in `pTraces::counterScoped::*`.

## JIT Index Hints
```bash
# Find all enum definitions
rg -n "^enum class" src/libcommon/utility/cstypes.h src/libcommon/utility/types.h

# Find toString() for a specific enum
rg -n "toString.*ExitCode\|toString.*SyncStep" src/libcommon/utility/utility.cpp

# Find all RequestNum values
rg -n "REQUEST_NUM\|RequestNum::" src/libcommon/comm.h

# Find all info classes
rg -ln "toDynamicStruct\|fromDynamicStruct" src/libcommon/info/

# Find usages of ExitInfo across the codebase
rg -n "ExitInfo" src/ -g "*.h" -l

# Find all Sentry trace names
rg -n "PTraceName::" src/libcommon/log/sentry/
```

## Common Gotchas
- `SyncName` is `std::wstring` on Windows and `std::string` on Unix. Never compare directly with `std::string` literals on Windows — always use the `Str("literal")` macro.
- `OperationType` and `InconsistencyType` are bitmask enums — use `|` and `&`, not `==`, for multi-flag tests.
- `DbNodeId` (SQLite ROWID) and `NodeId` (server string ID) are completely different. `DbNodeId` must never be exposed in the IPC layer; use the server-assigned `NodeId`.
- All `info/` DTOs must keep `toDynamicStruct` and `QDataStream` operators in sync — forgetting one breaks either JSON or binary IPC.

## Pre-PR Checks
```bash
cmake --build build-macos --target kDrive_test_common && ./build-macos/bin/kDrive_test_common
```
