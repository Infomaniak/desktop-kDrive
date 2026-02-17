# libparms — Parameters Database

## Package Identity
Persistent configuration store for all application-level state: users, accounts, drives, sync configurations, exclusion templates, error records, and upload session tokens. Backed by SQLite via `libcommonserver`'s `SqliteDb`. All CRUD operations are centralized here.

## Key Files
- Main DB class: `src/libparms/db/parmsdb.h`
- DB implementation: `src/libparms/db/parmsdb.cpp`
- Schema version management: `src/libparms/db/parmsdb.h` (versioning via `version` parameter passed to `instance()` and validated in `checkConnect(version)`)

## Data Model
Key entities and their primary key:
- **User** — `dbId` (local), `userId` (server-assigned)
- **Account** — `dbId`, `accountId` (server-assigned), linked to `User`
- **Drive** — `dbId`, `driveId` (server-assigned), linked to `Account`
- **Sync** — `dbId`, linked to `Drive`; holds `localPath` + `drivePath`
- **ExclusionTemplate** — pattern + warning flag
- **Error** — sync errors with `ExitCode` / `ExitCause` + timestamps
- **UploadSessionToken** — resumable upload state

## Patterns & Conventions
- All public methods on `ParmsDb` follow the pattern: `bool insertXxx(const Xxx &entity)` / `bool selectXxx(DbId id, Xxx &out, bool &found)` / `bool updateXxx(const Xxx &entity, bool &found)` / `bool deleteXxx(DbId id, bool &found)`.
- **Never** modify `ParmsDb` schema without incrementing the schema version (passed as `version` to `instance()` and validated in `checkConnect(version)`) and adding a migration in the upgrade path.
- Use `ParmsDb::instance()` singleton accessor from server/syncengine code.
- `ParmsDb` is distinct from `SyncDb` (per-sync file tree in `libsyncengine/db/`). Don't confuse them.
- DO: Add new entities following the existing CRUD pattern — see `src/libparms/db/parmsdb.h` for reference signatures.
- DON'T: Store sync engine tree state in `ParmsDb`. That belongs in `SyncDb`.

## JIT Index Hints
```bash
# Find all entity types
rg -n "struct .* \{" src/libcommon/info/ -g "*.h"

# Find all ParmsDb CRUD methods
rg -n "bool ParmsDb::" src/libparms/db/parmsdb.cpp | head -60

# Find schema version and migration
rg -n "upgradeDb|upgrade\|migration\|checkConnect" src/libparms/db/parmsdb.cpp

# Find tests for a specific entity
rg -rn "TestParmsDb\|testUser\|testDrive" test/libparms/
```

## Common Gotchas
- `ParmsDb` is **not** thread-safe for concurrent writes. The server serializes all write access.
- Schema migrations are **one-way**. Downgrades are not supported; changing the schema requires a new version and migration code.
- `dbId` is an internal SQLite `ROWID` — never expose it to the server API or GUI as a stable identifier; use the server-assigned `userId`/`driveId`/etc.

## Pre-PR Checks
```bash
cmake --build build-macos --target kDrive_test_parms && ./build-macos/bin/kDrive_test_parms
```
