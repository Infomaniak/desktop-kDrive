# test/libsyncengine — Sync Engine Tests

## Package Identity
Test suite for `libsyncengine`. Contains unit tests, integration tests (require live kDrive API credentials), and benchmark tests. Uses CppUnit. Mirrors the structure of `src/libsyncengine/`.

## Run Tests
```bash
# Run all sync engine tests
./build-macos/bin/kDrive_test_syncengine

# Run a single test suite
./build-macos/bin/kDrive_test_syncengine --suite=TestConflictFinderWorker

# Run extended/integration tests (requires credentials)
KDRIVE_TEST_CI_RUNNING_ON_CI=true \
KDRIVE_TEST_CI_API_TOKEN=<token> \
KDRIVE_TEST_CI_DRIVE_ID=<id> \
./build-macos/bin/kDrive_test_syncengine

# Extended nightly tests
KDRIVE_TEST_CI_EXTENDED_TEST=true ./build-macos/bin/kDrive_test_syncengine
```

## Test Structure
| Directory | Content |
|---|---|
| `test/libsyncengine/db/` | SyncDb CRUD tests |
| `test/libsyncengine/jobs/` | Job classes (upload, download, etc.) |
| `test/libsyncengine/propagation/` | Executor and operation sorter tests |
| `test/libsyncengine/reconciliation/` | Conflict finder/resolver tests |
| `test/libsyncengine/sync_operation/` | Operation generator tests |
| `test/libsyncengine/update_detection/` | FSO and update tree tests |
| `test/libsyncengine/integration/` | End-to-end sync scenarios (need API credentials) |
| `test/libsyncengine/benchmark/` | Performance benchmarks |

## Patterns & Conventions
- All test classes inherit `CppUnit::TestCase`. Register with `CPPUNIT_TEST_SUITE` / `CPPUNIT_TEST_SUITE_END`.
- Use `CPPUNIT_ASSERT`, `CPPUNIT_ASSERT_EQUAL`, `CPPUNIT_ASSERT_THROW` macros.
- **Shared helpers:** `test/test_utility/testbase.h` — `TestBase` provides `setUp`/`tearDown` with temp dirs and log init. Inherit from it.
- **Sync situations:** Use `TestSituationGenerator` (`test/test_classes/`) to set up complex sync states instead of manual DB insertion.
- **Mocks:** Use interfaces from `test/mocks/` — `MockSyncDb`, `MockVfs`, etc. — for unit tests that shouldn't hit real I/O.
- **Integration tests** must guard network calls with `if (CommonUtility::envVarValue("KDRIVE_TEST_CI_RUNNING_ON_CI") != "true") CPPUNIT_SKIP()`.
- DO: Co-locate unit tests with the feature they test (mirrors `src/libsyncengine/` layout).
- DON'T: Hard-code absolute paths or drive IDs in test files. Use env vars or `TestBase` temp dir utilities.

## JIT Index Hints
```bash
# Find test suite for a specific worker
rg -rn "class Test.*Worker" test/libsyncengine/ -g "*.h"

# Find tests for conflict resolution
rg -rni "CPPUNIT_TEST.*conflict|class TestConflict" test/libsyncengine/

# Find integration test guards
rg -n "KDRIVE_TEST_CI_RUNNING_ON_CI|CPPUNIT_SKIP" test/libsyncengine/integration/

# Find all benchmark tests
find test/libsyncengine/benchmark -name "*.cpp"
```

## Common Gotchas
- Integration tests create real files on kDrive — always clean up in `tearDown()`.
- `CPPUNIT_SKIP()` skips a test silently; use it for tests that require unavailable credentials, not to hide broken tests.
- Test binaries are built only when `BUILD_UNIT_TESTS=ON` is passed to CMake.
- Log output goes to `$TMPDIR/kDrive-logdir/` — check there first when a test fails with no assertion message.
