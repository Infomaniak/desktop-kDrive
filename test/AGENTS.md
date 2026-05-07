# test/ — Test Infrastructure

## Overview
All tests for the kDrive desktop client live here. The framework is **CppUnit**. Each source library has a
matching test sub-directory that compiles into its own standalone binary. Tests are pure C++20; no Qt test
framework is used.

Each test binary is built only when CMake is configured with `-DBUILD_UNIT_TESTS=ON`.

## Test Binaries

| Binary | Source directory | Links against |
|---|---|---|
| `kDrive_test_common` | `test/libcommon/` | `libcommonserver` |
| `kDrive_test_common_server` | `test/libcommonserver/` | `libcommonserver` |
| `kDrive_test_parms` | `test/libparms/` | `libparms` |
| `kDrive_test_syncengine` | `test/libsyncengine/` | `libsyncengine` |
| `kDrive_test_server` | `test/server/` | `libsyncengine` + Qt6 + server sources |

## Build & Run

```bash
# Configure with tests enabled (example: Linux, adapt path for your platform)
cmake -B build-linux -DCMAKE_BUILD_TYPE=Debug -DBUILD_UNIT_TESTS=ON
cmake --build build-linux --parallel

# Run all tests for one binary
./build-linux/bin/kDrive_test_syncengine
./build-linux/bin/kDrive_test_common
./build-linux/bin/kDrive_test_common_server
./build-linux/bin/kDrive_test_parms
./build-linux/bin/kDrive_test_server

# Run a single test suite within a binary (CppUnit --suite flag)
./build-linux/bin/kDrive_test_syncengine --suite=TestConflictFinderWorker
./build-linux/bin/kDrive_test_common --suite=TestIo

# Run integration/CI tests (require live API credentials)
KDRIVE_TEST_CI_RUNNING_ON_CI=true \
KDRIVE_TEST_CI_API_TOKEN=<token> \
KDRIVE_TEST_CI_DRIVE_ID=<id> \
KDRIVE_TEST_CI_USER_ID=<id> \
KDRIVE_TEST_CI_ACCOUNT_ID=<id> \
KDRIVE_TEST_CI_REMOTE_DIR_ID=<id> \
KDRIVE_TEST_CI_REMOTE_PATH=<path> \
./build-linux/bin/kDrive_test_syncengine

# Run extended nightly tests (network-heavy, larger files)
KDRIVE_TEST_CI_EXTENDED_TEST=true ./build-linux/bin/kDrive_test_syncengine
```

Log output goes to `$TMPDIR/kDrive-logdir/` — check there first when a test fails with no assertion message.

## How to Add a New Test Case

### Step 1 — Copy the template

The canonical starting point is `test/cppunit_test_template.h` and `test/cppunit_test_template.cpp`.

Replace `TestClass` / `testMyFunc` / `MyClass` with your actual names. Naming convention: `Test<ClassUnderTest>`.

Minimum viable header (`test/libcommon/utility/testmynewclass.h`):

```cpp
#pragma once
#include "testincludes.h"
#include "libcommon/utility/mynewclass.h"

namespace KDC {

class TestMyNewClass : public CppUnit::TestFixture, public TestBase {
    CPPUNIT_TEST_SUITE(TestMyNewClass);
    CPPUNIT_TEST(testSomeFeature);
    CPPUNIT_TEST_SUITE_END();

  public:
    void setUp() override;
    void tearDown() override;

  protected:
    void testSomeFeature();
};

} // namespace KDC
```

Minimum viable implementation (`test/libcommon/utility/testmynewclass.cpp`):

```cpp
#include "testmynewclass.h"

namespace KDC {

void TestMyNewClass::setUp() { /* init */ }
void TestMyNewClass::tearDown() { /* cleanup */ }

void TestMyNewClass::testSomeFeature() {
    CPPUNIT_ASSERT_EQUAL(expected, actual);
}

} // namespace KDC
```

### Step 2 — Register the suite

Open the `test.cpp` file in the target sub-directory (e.g. `test/libcommon/test.cpp`). Add:

```cpp
#include "utility/testmynewclass.h"

// inside the KDC namespace block:
CPPUNIT_TEST_SUITE_REGISTRATION(TestMyNewClass);
```

Example registration from `test/libcommon/test.cpp`:
```cpp
CPPUNIT_TEST_SUITE_REGISTRATION(TestUtility);
CPPUNIT_TEST_SUITE_REGISTRATION(TestTypes);
```

### Step 3 — Add sources to CMakeLists.txt

Open the matching `CMakeLists.txt` (e.g. `test/libcommon/CMakeLists.txt`) and append to the `_SRCS` list:

```cmake
utility/testmynewclass.h utility/testmynewclass.cpp
```

Existing example pattern (from `test/libcommon/CMakeLists.txt`):
```cmake
utility/testutility.cpp utility/testutility.h
utility/testtypes.cpp utility/testtypes.h
```

Always list `.h` alongside `.cpp` — this is the convention throughout the project.

### Step 4 — Rebuild

```bash
cmake --build build-linux --parallel
./build-linux/bin/kDrive_test_common --suite=TestMyNewClass
```

## Directory Roles

| Directory | Role |
|---|---|
| `test/libcommon/` | Tests for `src/libcommon/` — I/O helpers, types, logging, utility, Sentry handler, API token |
| `test/libcommonserver/` | Tests for `src/libcommonserver/` — server-side DB, utility |
| `test/libparms/` | Tests for `src/libparms/` — parameters database CRUD |
| `test/libsyncengine/` | Tests for `src/libsyncengine/` — full sync pipeline; **has its own [AGENTS.md](libsyncengine/AGENTS.md)** |
| `test/server/` | Tests for `src/server/` — app server, comm channel (socket/pipe), GUI jobs, updater, VFS (macOS/Windows) |
| `test/mocks/` | Reusable mock classes (header-only). No own CMakeLists; consumed by other test targets |
| `test/test_classes/` | Shared test helpers requiring heavy setup: `SyncPalTest` and `TestSituationGenerator` |
| `test/test_utility/` | Lightweight shared utilities: `TestBase`, `testhelpers`, temp directories, data extractor |
| `test/test_ci/` | Static fixture data used by tests: sample files, directories, symlinks |

> `test/libsyncengine/` has its own detailed AGENTS.md → [test/libsyncengine/AGENTS.md](libsyncengine/AGENTS.md)

## Shared Utilities (`test/test_utility/`)

### TestBase (`test/test_utility/testbase.h`)
Lightweight timing mixin. Inherit alongside `CppUnit::TestFixture` to get per-test duration printed when a
test exceeds 50 ms.

```cpp
class TestMyClass : public CppUnit::TestFixture, public TestBase { ... };
```

### testhelpers (`test/test_utility/testhelpers.h`)
Stateless free functions in `KDC::testhelpers`:
- `isRunningOnCI()` — returns true when `KDRIVE_TEST_CI_RUNNING_ON_CI` is set.
- `isExtendedTest()` — returns true when `KDRIVE_TEST_CI_EXTENDED_TEST` is set.
- `generateTestFile(path, size)` — creates a file of the given size.
- `generateBigFiles(dir, sizeMB, count)` — bulk file generation for bandwidth tests.
- `loadEnvVariable(key, mandatory)` — reads env var; throws if mandatory and missing.
- `TestVariables` struct — loads all CI env vars in one shot (used in integration tests).
- Platform-specific implementations: `testhelpers_mac.cpp`, `testhelpers_linux.cpp`, `testhelpers_win.cpp`.

### LocalTemporaryDirectory (`test/test_utility/localtemporarydirectory.h`)
RAII wrapper that creates and auto-deletes a temp directory under the system temp path.

```cpp
LocalTemporaryDirectory tmpDir("mysuite");
SyncPath workDir = tmpDir.path(); // auto-deleted on destruction
```

### RemoteTemporaryDirectory (`test/test_utility/remotetemporarydirectory.h`)
Creates a directory on the live kDrive API and deletes it in its destructor. Only usable in integration
tests (requires CI credentials). Always call `setDeleted()` if you manually deleted it to avoid a
double-delete in `tearDown`.

### TimeoutHelper (`test/test_utility/timeouthelper.h`)
Helper for tests that must wait for an async condition with a deadline.

### DataExtractor (`test/test_utility/dataextractor.h`)
Utility to extract and inspect data from sync structures during test assertions.

## Shared Test Classes (`test/test_classes/`)

### SyncPalTest (`test/test_classes/syncpaltest.h`)
Subclass of `SyncPal` with test-friendly accessors. Use it instead of instantiating `SyncPal` directly
when you need to inspect internal state. Example usage: `test/libsyncengine/syncpal/testsyncpal.h`.

### TestSituationGenerator (`test/test_classes/testsituationgenerator.h`)
Builds complex sync states (local/remote trees, DB entries, conflict situations) without manual DB
insertion. Use it to set up multi-file scenarios in one call. Example usage:
`test/libsyncengine/reconciliation/`.

## Mocks (`test/mocks/`)

Header-only mock classes. No own CMakeLists — each consuming test target lists them directly in its
`_SRCS`.

| Mock | Path | What it mocks |
|---|---|---|
| `MockDb` | `test/mocks/libcommonserver/db/mockdb.h` | `Db` — generates collision-free DB file names for parallel test isolation |
| `MockVfs<T>` | `test/mocks/libsyncengine/vfs/mockvfs.h` | Any `Vfs`-derived class — override individual VFS operations via `setMock*` / `resetMock*` lambdas |
| `MockLogUploadJob` | `test/mocks/libsyncengine/jobs/network/API_v2/mockloguploadjob.h` | `LogUploadJob` — intercept `init`, `archive`, `upload`, `finalize` individually |

### Using MockVfs

```cpp
#include "mocks/libsyncengine/vfs/mockvfs.h"

MockVfs<VfsOff> vfs(params);
vfs.setMockCreatePlaceholder([](const SyncPath &, const SyncFileItem &) {
    return ExitInfo{ExitCode::Ok};
});
// call code under test that uses vfs ...
vfs.resetMockCreatePlaceholder(); // restore real implementation
```

### Using MockDb

```cpp
#include "mocks/libcommonserver/db/mockdb.h"

bool alreadyExist = false;
auto dbPath = MockDb::makeDbName(alreadyExist); // unique path, safe for parallel runs
MockDb db(dbPath);
```

## Integration Test Guards

Two equivalent guard patterns coexist in the codebase. Use either consistently within a file:

```cpp
// Pattern A — explicit env var check
if (CommonUtility::envVarValue("KDRIVE_TEST_CI_RUNNING_ON_CI") != "true") CPPUNIT_SKIP();

// Pattern B — testhelpers wrapper (preferred in new code)
if (!testhelpers::isRunningOnCI()) CPPUNIT_SKIP();

// Extended tests (nightly, larger transfers)
if (!testhelpers::isExtendedTest()) return;
```

## Static Test Fixtures (`test/test_ci/`)

Read-only fixture files used by IO and file-system tests:
- `test_ci/test_local_FSO/` — local file system observer fixtures
- `test_ci/test_remote_FSO/` — remote file system observer fixtures
- `test_ci/test_pictures/` — sample image files
- `test_ci/dummy_dir/`, `many_files_dir/` — directory structure fixtures
- `test_ci/test.lnk` — Windows shortcut fixture

Do not modify these files. Tests that need mutable data must copy fixtures into a `LocalTemporaryDirectory`.

## Patterns & Conventions

- All test classes are in the `KDC` namespace.
- Inherit `CppUnit::TestFixture` (required) and `TestBase` (optional timing).
- `CPPUNIT_TEST_SUITE_REGISTRATION` goes in the per-binary `test.cpp`, not in the test class file.
- Use `CPPUNIT_ASSERT`, `CPPUNIT_ASSERT_EQUAL`, `CPPUNIT_ASSERT_THROW`, `CPPUNIT_SKIP`.
- DO: Mirror `src/` layout inside the matching `test/` sub-directory.
  - `src/libcommon/utility/foo.h` -> `test/libcommon/utility/testfoo.h`
- DO: Use `LocalTemporaryDirectory` for any test that creates files on disk.
- DO: Call `setDeleted()` on `RemoteTemporaryDirectory` if you manually removed the remote dir.
- DON'T: Hard-code absolute paths, user IDs, or drive IDs. Use `testhelpers::loadEnvVariable` or
  the `TestVariables` struct.
- DON'T: Use `std::cout` for logging. Use `LOG_INFO` / `LOG_DEBUG` from log4cplus.
- DON'T: Leave integration tests without a `CPPUNIT_SKIP()` guard — they will fail on developer machines.
- DON'T: Comment out a `CPPUNIT_TEST_SUITE_REGISTRATION` to disable a benchmark permanently; leave a
  comment explaining why it is disabled (see `test/libsyncengine/test.cpp` line 84, 95).

## Common Gotchas

- Integration tests create real files on kDrive — always clean up in `tearDown()` using
  `RemoteTemporaryDirectory` or explicit API calls.
- `CPPUNIT_SKIP()` skips silently and reports OK. Never use it to hide broken unit tests — only for
  missing credentials.
- The `test/server/` CMakeLists re-compiles `src/server/` sources directly (not a shared lib), so
  changes to server source files require rebuilding `kDrive_test_server` as well.
- Platform-conditional tests use `#if defined(KD_MACOS) / KD_WINDOWS / KD_LINUX` — always check
  that new guards are consistent with the existing ones in the same file.
- CppUnit registers tests via a global factory. Forgetting `CPPUNIT_TEST_SUITE_REGISTRATION` in
  `test.cpp` means the suite compiles but never runs — it will not produce a failure.

## JIT Index Hints

```bash
# Find all test suite class declarations across test/
rg -rn "class Test[A-Z]" ./test/ -g "*.h"

# Find which binary a test suite belongs to (check test.cpp registrations)
rg -rn "CPPUNIT_TEST_SUITE_REGISTRATION" ./test/ -g "test.cpp"

# Find an existing test to copy as a model (e.g. for a utility class)
rg -rn "class Test.*Utility" ./test/ -g "*.h"

# Find all integration test guards
rg -rn "isRunningOnCI|KDRIVE_TEST_CI_RUNNING_ON_CI|CPPUNIT_SKIP" ./test/ -g "*.cpp"

# Find all mock usages
rg -rn "#include.*mock" ./test/ -g "*.h" -i

# Find tests that use LocalTemporaryDirectory (good models for file-system tests)
rg -rn "LocalTemporaryDirectory" ./test/ -g "*.h" -l

# Find tests for a specific class name
rg -rn "class TestSyncPal" ./test/ -g "*.h"

# Find all CPPUNIT_TEST macros in a suite to understand test coverage
rg -n "CPPUNIT_TEST(" ./test/libsyncengine/reconciliation/ -g "*.h"
```
