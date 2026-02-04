# kDrive Desktop - Copilot Instructions

## Repository Overview
**kDrive Desktop** is a cross-platform C++20/Qt 6.2.3 sync client for Infomaniak's cloud storage. Originally an Owncloud fork (2019), core rewritten by 2023. ~11K lines of code.

**Stack:** CMake 3.16+, Conan 2.x, Qt 6.2.3, Poco, xxHash, Sentry, log4cplus, CppUnit, libzip  
**Platforms:** Linux (Ubuntu 22.04+), macOS 10.15+, Windows 10+  
**Extensions:** LiteSync for on-demand downloads (Windows/macOS)

## Build Requirements

### Prerequisites (All Platforms)
1. **Clone with submodules:** `git submodule update --init --recursive`
2. **Set KDRIVE_THEME_DIR** CMake variable to `<repo_root>/infomaniak` (REQUIRED)
3. **Conan dependencies:** xxHash, log4cplus, OpenSSL, zlib
4. **Manual dependencies:** Poco, Sentry, libzip, CppUnit, Qt 6.2.3

### Conan Setup
```bash
# Install Conan in virtual environment
python3 -m venv .venv && source .venv/bin/activate  # Windows: .\.venv\Scripts\Activate.ps1
pip install --upgrade pip && pip install conan

# Create profile
conan profile detect
# Edit ~/.conan2/profiles/default with correct settings (see platform READMEs)

# Install dependencies
./infomaniak-build-tools/conan/build_dependencies.sh [Debug|Release] [--output-dir=<dir>]
```

### Linux (Ubuntu 22.04+)
**Packages:** `clang-18+ libgl1-mesa-dev libsqlite3-dev libsecret-1-dev libxcb-cursor0`  
**Qt 6.2.3:** Desktop gcc 64-bit, Qt5Compat, WebEngine, Positioning, WebChannel, WebView

**Key Steps:**
1. Conan install → 2. **Source `conanrun.sh` before Poco build** → 3. Build Poco 1.13.3, Sentry 0.7.9, CppUnit, libzip 1.10.1
2. **CI:** `./infomaniak-build-tools/linux/build-ci-amd64.sh -u -t release`  
**Release:** `./infomaniak-build-tools/linux/build-release-amd64.sh` (Podman-based)  
**Output:** `build-linux/install/*.AppImage` or `build-linux-amd64/client/install/*.AppImage`

### macOS (10.15+)
**Required:** Xcode, Qt 6.2.3, Sparkle 2.6.4, Packages, homebrew  
**Universal Binary:** x86_64 + arm64 (except Debug w/ tests - single arch only)

**Key Steps:**
1. Disable SIP for extension debugging
2. Conan with `arch=armv8|x86_64` → Build Sentry (universal), CPPUnit (single arch!), Poco (universal), libzip+zstd (universal)
3. **CI:** `./infomaniak-build-tools/macos/build-ci.sh` | **Release:** `.../build-release.sh`  
**Output:** `build-macos/client/install/*.pkg`  
**Critical:** CPPUnit single-arch only - use current arch when `BUILD_UNIT_TESTS=ON`

### Windows (10+)
**Required:** VS2019 + (VS2022 OR VS2025), Qt 6.2.3 MSVC 2019, NSIS 3.03, 7za, icoutils  
**CRITICAL:** Use `x64 Native Tools Command Prompt` with admin rights

**Key Steps:**
1. **Build VFS Extension FIRST:** Open `extensions/windows/cfapi/kDriveExt.sln` in VS2019, build/deploy x64
2. Conan → Install Sentry, Poco, CPPUnit, zlib, libzip
3. **CI:** `powershell -File "./infomaniak-build-tools/windows/build-drive.ps1" -ci -unitTests`  
**Release:** `powershell "./infomaniak-build-tools/windows/build-drive.ps1" -ci -ext -msi -clean remake -upload`

**Poco Build Fix:** Add `C:\Program Files\OpenSSL-Win64\lib` to Library Directories for Crypto/JWT/NetSSL in VS2019

## Testing
**Suites:** test_common, test_common_server, test_server, test_syncengine, test_parms  
**Run:** `./infomaniak-build-tools/run-test.sh <build_dir> <test_name>` (Linux/macOS)  
**CI:** Tests auto-run after build (requires env vars: `KDRIVE_TEST_CI_*`, `QT_QPA_PLATFORM=offscreen`)

## Code Standards
**Format before commit:** `./run_clang-format.sh` (Google-based, 130 char limit, 4-space indent, C++20)  
**Excludes:** `src/3rdparty/`, `build-*`

## Project Structure

### Key Directories
```
src/
├── libcommon/          # Common utilities shared across components
├── libcommonserver/    # Server-specific common code (network, db, vfs, utility)
├── libcommongui/       # GUI common components
├── libsyncengine/      # Core synchronization engine (12 subdirs)
├── libparms/           # Parameters management
├── server/             # Server application (comm, requests, migration, updater, vfs)
├── gui/                # Main GUI application (Qt widgets)
├── gui4/               # Next-gen GUI (Windows only - WinUI)
├── front/              # Platform-specific frontends (macOS)
├── 3rdparty/           # Third-party libraries (DO NOT MODIFY)
└── common/             # Common headers

extensions/
├── windows/cfapi/      # Windows Cloud Files API extension (C#/.NET)
└── (macOS extensions in src/front/macOS)

infomaniak/             # Theme and branding (REQUIRED for CMAKE)
├── kDrive.cmake        # Application configuration
└── theme/              # Icons, images, resources

infomaniak-build-tools/ # Build scripts for all platforms
├── linux/              # Linux build scripts
├── macos/              # macOS build scripts
├── windows/            # Windows build scripts (PowerShell)
└── conan/              # Conan dependency scripts

test/                   # Unit test infrastructure
```

### Key Config Files
**CMakeLists.txt** (main build), **conanfile.py** (deps), **VERSION.cmake** (version parser), **THEME.cmake** (theme loader), **version.json** (version data), **.clang-format** (format rules)

## CI/CD Workflows
**Main CI** (`kdrive-desktop-ci.yml`): PR to non-release → Build (3 platforms parallel) → Test (5 suites × 3 platforms = 15 jobs). Dependencies cached separately.  
**Release:** `kdrive-desktop-release.yml` (signed builds), `kdrive-desktop-msi-build.yml` (Windows MSI)  
**Extended:** `build-and-run-extended-tests.yml` (full integration tests)

## Common Issues & Solutions
1. **Poco build:** Source `conanrun.sh/.bat` BEFORE building Poco (needs Conan's OpenSSL)
2. **Linux Qt plugin:** Install `libxcb-cursor0`
3. **Windows missing DLLs:** Copy from Poco/Sentry/OpenSSL/zlib bin dirs to output
4. **macOS universal fail:** CPPUnit single-arch only - set `CMAKE_OSX_ARCHITECTURES` to current when `BUILD_UNIT_TESTS=ON`
5. **Poco link (Windows):** Add `C:\Program Files\OpenSSL-Win64\lib` to Crypto/JWT/NetSSL in VS2019

**Debug Paths:** Linux: `build-linux/build/bin/`, logs `/tmp/kDrive-logdir/`, config `~/.config/kDrive` | macOS: `build-macos/client/bin/` (run `cmake --install` once) | Windows: Set VFS vars

**Required CMake Vars:**
```cmake
KDRIVE_THEME_DIR=<repo_root>/infomaniak  # CRITICAL
APPLICATION_CLIENT_EXECUTABLE=kdrive_client
CMAKE_PREFIX_PATH=<Qt>/6.2.3/<platform>
CMAKE_BUILD_TYPE=Debug|Release|RelWithDebInfo
BUILD_UNIT_TESTS=ON|OFF
# macOS: TEAM_IDENTIFIER_PREFIX=864VDCS2QY
# Windows: VFS_DIRECTORY, VFS_STATIC_LIBRARY, ZLIB_*
```

## Important Notes
1. **Never modify `src/3rdparty/`** (external libs)
2. **Version changes:** Update `version.json` (parsed by VERSION.cmake)
3. **Conan migration ongoing:** Poco, Sentry, libzip, Qt, CPPUnit still manual
4. **Platform macros:** `KD_MACOS`, `KD_WINDOWS`, `KD_LINUX` (CMakeLists.txt)
5. **Windows:** Build VFS extension before main app
6. **Build dirs:** `build-linux/`, `build-macos/`, `build-windows/` (git-ignored)

**Trust these instructions.** Search only if incomplete/incorrect. Build is complex - follow steps precisely.
