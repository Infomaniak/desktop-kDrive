# libcommonserver — Server-Side Shared Utilities

## Package Identity
Shared library providing server-process utilities: platform-abstracted filesystem I/O, SQLite wrapper, VFS plugin interface, log4cplus logger setup, keychain manager, and network proxy configuration. Heavy platform-specific branching (`_mac.mm` / `_win.cpp` / `_linux.cpp`).

## Key Files
- Filesystem I/O (platform-abstracted): `src/libcommonserver/io/iohelper.h`
- Platform implementations: `src/libcommonserver/io/iohelper_mac.mm`, `iohelper_win.cpp`, `iohelper_linux.cpp`
- SQLite DB base class: `src/libcommonserver/db/sqlitedb.h`
- SQLite query wrapper: `src/libcommonserver/db/sqlitequery.h`
- Logger setup: `src/libcommonserver/log/log.h`
- Keychain manager: `src/libcommonserver/keychainmanager/keychainmanager.h`
- VFS plugin interface: `src/libcommonserver/vfs/vfs.h`
- Network proxy: `src/libcommonserver/network/proxy.h`
- Platform utilities: `src/libcommonserver/utility/utility_mac.mm`, `utility_win.cpp`, `utility_linux.cpp`

## Patterns & Conventions
- **Platform branching:** Always provide all three implementations (`_mac.mm`, `_win.cpp`, `_linux.cpp`) when adding platform-specific code. Use `#ifdef Q_OS_*` only for small inline differences; prefer separate files for substantial differences.
- **IO operations:** All file system interactions go through `IoHelper`. Never use `std::filesystem` or POSIX calls directly in `libsyncengine` or `server` — call `IoHelper`.
- **SQLite:** Use `SqliteDb` + `SqliteQuery` wrappers. Never call `sqlite3_*` functions directly outside `libcommonserver/db/`.
- **Logging:** Logger initialization is done once in `Log::init()`. Child libraries get their logger via `Log::instance()->getLogger("libraryName")`.
- **Keychain:** Credentials are stored/retrieved via `KeychainManager`. Never cache raw passwords in memory long-term.
- DO: Add new platform-specific file ops to `IoHelper` — one interface, three implementations.
- DON'T: Add UI or Qt Widgets dependencies here. This library is server-side only.

## JIT Index Hints
```bash
# Find all IoHelper methods
rg -n "static.*IoHelper::" src/libcommonserver/io/iohelper.h

# Find platform-specific IO implementations
rg --files src/libcommonserver/io/ -g "iohelper_*"

# Find DB schema for a table
rg -n "CREATE TABLE" src/libcommonserver/db/

# Find VFS plugin interface methods
rg -n "virtual" src/libcommonserver/vfs/vfs.h
```

## Common Gotchas
- `IoHelper` methods return `bool` + set an `IoError` out-param. Always check the return value AND the error code.
- On macOS, file operations may require security-scoped bookmarks for sandboxed contexts (Finder extension).
- `SqliteDb` is not thread-safe. Each thread that uses SQLite must have its own `SqliteDb` instance or serialize access.

## Pre-PR Checks
```bash
cmake --build build-macos --target kDrive_test_common_server && ./build-macos/test/libcommonserver/kDrive_test_common_server
```
