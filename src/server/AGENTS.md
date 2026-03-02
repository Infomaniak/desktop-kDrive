# server — Background Server Process

## Package Identity
Long-running background daemon that owns all sync state. Communicates with the GUI process via IPC (XPC on macOS, TCP sockets on Windows/Linux). Communicates with shell extensions via XPC on macOS, named pipes on Windows/Linux. Loads the VFS plugin, manages auto-updates, and dispatches GUI-requested operations via typed job objects.

## Key Files
- IPC communication manager: `src/server/comm/guicommserver.h` (GUI socket layer), `src/server/comm/socketcommserver.h` (low-level socket)
- GUI job dispatcher: `src/server/comm/guijobs/`
- VFS plugin manager: `src/server/vfs/`
- Auto-updater (macOS Sparkle): `src/server/updater/sparkleupdater.h`
- Auto-updater (Windows): `src/server/updater/windowsupdater.h`
- Auto-updater (Linux): `src/server/updater/linuxupdater.h`
- Migration from legacy config: `src/server/migration/`

## IPC Architecture
The GUI sends typed **job requests** to the server over the IPC channel. Each capability is a separate job class.

Platform breakdown:
- **macOS** → XPC (`GuiCommServer extends XPCCommServer`, see `xpccommserver_mac.h`)
- **Windows / Linux** → TCP socket (`GuiCommServer extends SocketCommServer`, see `socketcommserver.h`)

```bash
# Find all GUI-facing job classes
rg -n "class .*Job" src/server/comm/guijobs/ -g "*.h"
```

Pattern: `src/server/comm/guijobs/abstractguijob.{h,cpp}` — each job has a `run()` method that performs the operation and sends a typed response back to the GUI.

## Patterns & Conventions
- **IPC messages** are defined in `src/libcommon/comm.h`. Any new message type must be added there and handled symmetrically in both `src/server/comm/guicommserver.cpp` and `src/libcommongui/commclient.cpp`.
- **VFS plugin** is loaded dynamically at runtime. VFS-related code must guard against the plugin being absent (`if (_vfs)`).
- **Platform-specific server code** follows the `_mac.mm` / `_win.cpp` / `_linux.cpp` suffix convention (see `src/server/appserver_mac.mm`).
- **Logging:** use `LOG_*` macros with the `server` logger name.
- DO: Add new server-side features as a new `Job` class in `src/server/comm/guijobs/`.
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
- macOS uses **XPC** exclusively for GUI IPC; Windows/Linux use TCP sockets. They are separate implementations — changes to IPC logic may need to be applied to both `xpccommserver_mac.mm` and `socketcommserver.cpp`.
- The server process starts before the GUI; never assume GUI is alive when handling server-side events.
- VFS mode (LiteSync) changes the file representation on disk; IO operations in VFS mode must go through the VFS plugin API, not direct `std::filesystem` calls.

## Pre-PR Checks
```bash
cmake --build build-macos --target kDrive_test_server && ./build-macos/bin/kDrive_test_server
```
