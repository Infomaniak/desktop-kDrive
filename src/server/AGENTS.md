# server — Background Server Process

## Package Identity
Long-running background daemon that owns all sync state. Communicates with the GUI process via IPC (Unix sockets on Linux/macOS, named pipes on Windows, XPC on macOS). Loads the VFS plugin, manages auto-updates, and dispatches GUI-requested operations via typed job objects.

## Key Files
- IPC communication manager: `src/server/commserver.h`
- GUI job dispatcher: `src/server/requests/`
- VFS plugin manager: `src/server/vfs/`
- Auto-updater (macOS Sparkle): `src/server/updater/sparkleupdater.h`
- Auto-updater (Windows): `src/server/updater/windowsupdater.h`
- Auto-updater (Linux): `src/server/updater/linuxupdater.h`
- Migration from legacy config: `src/server/migration/`

## IPC Architecture
The GUI sends typed **job requests** to the server over the IPC channel. Each capability is a separate job class:

```bash
# Find all GUI-facing job classes
rg -n "class .*Job" src/server/requests/ -g "*.h"
```

Pattern: `src/server/requests/SyncAddJob.{h,cpp}` — each job has a `run()` method that performs the operation and sends a typed response back to the GUI.

## Patterns & Conventions
- **IPC messages** are defined in `src/libcommon/comm.h`. Any new message type must be added there and handled symmetrically in both `src/server/commserver.cpp` and `src/gui/commclient.cpp`.
- **VFS plugin** is loaded dynamically at runtime. VFS-related code must guard against the plugin being absent (`if (_vfs)`).
- **Platform-specific server code** follows the `_mac.mm` / `_win.cpp` / `_linux.cpp` suffix convention (see `src/server/appserver_mac.mm`).
- **Logging:** use `LOG_*` macros with the `server` logger name.
- DO: Add new server-side features as a new `Job` class in `src/server/requests/`.
- DON'T: Call sync engine internals directly from IPC handlers. Use the `SyncPal` public API.

## JIT Index Hints
```bash
# Find IPC message definitions
rg -n "enum.*Msg|struct.*Msg" src/libcommon/comm.h

# Find XPC-specific code
rg -ln "xpc" src/server/ -g "*.mm" -g "*.h"

# Find updater logic
rg -n "class.*Updater" src/server/updater/ -g "*.h"

# Find VFS integration points
rg -n "vfs|Vfs|VFS" src/server/ -g "*.h" -l
```

## Common Gotchas
- macOS uses **XPC** for inter-process communication in addition to the socket layer; changes to IPC on macOS require updating both layers.
- The server process starts before the GUI; never assume GUI is alive when handling server-side events.
- VFS mode (LiteSync) changes the file representation on disk; IO operations in VFS mode must go through the VFS plugin API, not direct `std::filesystem` calls.

## Pre-PR Checks
```bash
cmake --build build-macos --target kDrive_test_server && ./build-macos/test/server/kDrive_test_server
```
