# libsyncengine — Sync Engine

## Package Identity
Core sync library implementing the **Syncpal algorithm** (Shekow, 2019). Orchestrates a pipeline of workers that detect, reconcile, and propagate file-system changes between the local drive and the kDrive API. All production code; no UI.

## Setup & Build
```bash
# Build only this library
cmake --build build-macos --target kDrivesyncengine --parallel

# Run sync engine tests
./build-macos/bin/kDrive_test_syncengine

# Run a single test suite
./build-macos/bin/kDrive_test_syncengine --suite=TestSyncOpExecutor
```

## Architecture: The Syncpal Pipeline

```
FSO (filesystem observer)
  → UpdateTreeWorker      (build update tree from detected changes)
    → PlatformInconsistencyChecker
      → ConflictFinder
        → ConflictResolver
          → OperationGenerator
            → OperationSorter
              → Executor  (apply operations: upload/download/mkdir/delete/move)
```

All stages are `ISyncWorker` subclasses managed by `SyncPalWorker`. Each stage reads from the previous stage's output queue and writes to its own.

**Key entry point:** `src/libsyncengine/syncpal/syncpalworker.h` — the orchestrator that drives all workers.

## Key Files
- Pipeline orchestrator: `src/libsyncengine/syncpal/syncpalworker.h`
- Worker interface: `src/libsyncengine/syncpal/isyncworker.h`
- Conflict finder: `src/libsyncengine/reconciliation/conflict_finder/conflictfinderworker.h`
- Conflict resolver: `src/libsyncengine/reconciliation/conflict_resolver/conflictresolverworker.h`
- Operation executor: `src/libsyncengine/propagation/executor/executorworker.h`
- File system observer: `src/libsyncengine/update_detection/file_system_observer/`
- Job system: `src/libsyncengine/jobs/` (network + local jobs)
- Sync DB wrapper: `src/libsyncengine/db/syncdb.h`

## Patterns & Conventions
- **Workers** inherit `ISyncWorker`. Never call worker methods directly across stages; always use the shared output data structures.
- **Jobs** (in `jobs/`) model individual network API calls (upload, download, list, create, delete, move) and local file operations. Job classes inherit from a base `AbstractJob`.
- **Network jobs** live in `jobs/network/`; **local jobs** in `jobs/local/`. Don't mix.
- **DB access** goes through `SyncDb` (`db/syncdb.h`), never raw SQLite calls.
- **Error propagation:** Use `ExitCode` / `ExitCause` enums from `libcommon/utility/types.h`. Never throw exceptions across worker boundaries.
- **Progress tracking:** Update `ProgressInfo` in the executor, not in upstream workers.
- **Examples of job patterns:**
  - Upload: `src/libsyncengine/jobs/network/upload/uploadjob.h`
  - Download: `src/libsyncengine/jobs/network/download/downloadjob.h`
  - Directory listing: `src/libsyncengine/jobs/network/getfilelistjob.h`

## JIT Index Hints
```bash
# Find all worker classes
rg -n "class .*Worker.*: public ISyncWorker" src/libsyncengine/

# Find all job classes
rg -n "class .*Job" src/libsyncengine/jobs/ -g "*.h"

# Find conflict resolution logic
rg -n "ConflictType::" src/libsyncengine/reconciliation/

# Find operation types
rg -n "OperationType::" src/libsyncengine/

# Find DB schema operations
rg -n "CREATE TABLE|INSERT INTO|UPDATE.*SET" src/libsyncengine/db/
```

## Common Gotchas
- The pipeline is **single-threaded per sync** within each worker stage. Don't add concurrency inside a worker without understanding the locking model.
- `SyncDb` is **not** the same as `ParmsDb` (in `libparms/`). SyncDb stores the live file tree; ParmsDb stores user/account/drive configuration.
- File identity uses **node IDs** (server-assigned), not paths. Paths can change due to renames/moves; always track by node ID.
- The FSO runs on a **separate thread** from the pipeline workers; shared state must be protected.

## Pre-PR Checks
```bash
cmake --build build-macos --target kDrive_test_syncengine && ./build-macos/bin/kDrive_test_syncengine
```
