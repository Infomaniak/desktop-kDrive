# Conan Build System — kDrive Desktop

## Package Identity

C++ dependency management system using Conan 2.x for the kDrive desktop client.
Contains custom recipes (Qt, OpenSSL, Sentry, Poco, xxHash) and cross-platform build scripts.

## Structure

```
conan/
├── build_dependencies.sh       # Main build script (Linux/macOS)
├── build_dependencies.ps1      # Main build script (Windows)
├── common-utils.sh             # Shared shell utilities (find_conan_dependency_path, find_qt_conan_path)
├── find_conan_dep.ps1          # PowerShell equivalent of common-utils.sh
├── LegacyReadme.md             # Historical pre-Conan build instructions
└── recipes/                    # Custom Conan recipes
    ├── qt/all/conanfile.py             # Qt 6.x via Qt Online Installer
    ├── openssl-macos/all/conanfile.py  # OpenSSL universal binary for macOS (lipo)
    ├── sentry/all/conanfile.py         # Sentry Native SDK + Crashpad
    ├── poco/all/conanfile.py           # Poco C++ libraries (22+ components)
    └── xxhash/all/conanfile.py         # xxHash hashing library
```

## Setup & Run

```bash
# Build all dependencies (Linux/macOS)
./build_dependencies.sh --build-type Release

# Build with cache cleanup
./build_dependencies.sh --build-type Release --clean-cache

# Release build (infomaniak_release profile)
./build_dependencies.sh --build-type Release --make-release

# Build on Windows
.\build_dependencies.ps1

# Find an installed Conan package
source common-utils.sh
find_conan_dependency_path "openssl"
find_qt_conan_path
```

## Patterns & Conventions

### Recipe Structure

Every recipe follows a standardized layout:
```
recipes/{package_name}/
├── config.yml          # Supported versions → folder (always "all")
└── all/
    └── conanfile.py    # Single recipe handling all versions
```

- **config.yml**: mapping `{version}: {folder: "all"}`
- **conanfile.py**: class inheriting `ConanFile`, handles download/build/package

### Naming

| Element      | Convention            | Example                               |
|--------------|-----------------------|---------------------------------------|
| Package name | lowercase, hyphens    | `openssl-macos`, `xxhash`             |
| CMake target | TitleCase + namespace | `Qt6`, `Poco::Poco`, `xxHash::xxhash` |
| Components   | snake_case            | `poco_foundation`, `sentry_crashpad`  |
| Options      | snake_case            | `qt_login_type`, `enable_crypto`      |

### Architectures

- **Conan**: `armv8` (ARM64), `x86_64` (Intel)
- **macOS Release**: Universal binary `armv8|x86_64` (pipe-separated)
- **macOS Debug**: Current architecture only
- **Linux**: Native architecture only

### Package ID Overrides

Several recipes remove `build_type` from package_id to avoid unnecessary rebuilds:
- `qt`: same binary for Debug/Release
- `xxhash`: forces Release
- `openssl-macos`: removes compiler, build_type, shared, requires

## Dependency Graph

```
qt/6.x (standalone)

openssl-macos/3.2.4 (macOS only)
  └── zlib [>=1.2.11 <2]

xxhash/0.8.2
  └── ninja [>=1.11.1] (Windows, tool_requires)

sentry/0.7.10
  ├── qt [>=6 <7] (headers only, private)
  └── zlib [>=1.2.11 <2] (Linux)

poco/1.13.3
  ├── pcre2 [>=10.42 <11]
  ├── zlib [>=1.2.11 <2]
  ├── expat [>=2.6.2 <2.7.4]
  └── openssl(-macos)/3.2.4
```

## Platform-Specific Details

### macOS
- Universal binaries via `lipo` (Release only)
- Dedicated `openssl-macos` recipe (separate x86_64 + arm64 builds merged with lipo)
- Qt installer: DMG mount/unmount, FindWrapOpenGL.cmake patches
- `install_name_tool` for rpath fixes (`@loader_path`)

### Linux
- System package manager detection for Sentry dependencies (apt, yum, dnf, pacman, zypper, apk)
- Additional QtSerialPort module installation
- No universal binary support

### Windows
- MSVC version selection by Qt version (2019 for <=6.5.3, 2022 for >=6.8.3)
- Optional Qt Visual C++ Redistributables
- Ninja generator for xxHash
- User PATH management and SSL certificate handling in CI

## Key Files

- Main build script: `build_dependencies.sh:1` (Linux/macOS)
- Shared utilities: `common-utils.sh:1`
- Qt recipe (most complex): `recipes/qt/all/conanfile.py:1`
- macOS OpenSSL build: `recipes/openssl-macos/all/openssl_build.sh:1`
- Poco components (22+): `recipes/poco/all/conanfile.py:1`

## JIT Index Hints

```bash
# Find a recipe by name
rg -n "class.*ConanFile" recipes/
# Search for a Conan option
rg -n "options\s*=" recipes/ --type py
# Supported versions
rg -n "versions:" recipes/ --glob "config.yml"
# Find macOS-specific code
rg -n "Darwin|macos|apple" recipes/ --type py
# Find Windows-specific code
rg -n "Windows|msvc|MSVC" recipes/ --type py
# Qt modules installed
rg -n "qt\." recipes/qt/all/conanfile.py
```

## Common Gotchas

- **Qt login**: `ini` (default) prefers the runner's `qtaccount.ini` and clears environment credentials for the
  installer process. Outside CI, a missing file falls back to `envvars` only when `QT_INSTALLER_JWT_TOKEN` is non-empty.
  Explicit `envvars` uses this token and ignores environment email/password credentials. `cli` ignores environment
  credentials. GitHub Actions and `KDRIVE_TEST_CI_RUNNING_ON_CI=true` require `ini` and an existing account file, rejecting
  token or interactive login. Windows CI uses the same
  selection as macOS/Linux. Paths respect the current home directory, `APPDATA` on Windows, and `XDG_DATA_HOME` on Linux.
  The recipe centrally clears competing environment credentials for CI, release, and extended-test builds;
  the GitHub Qt JWT secret is no longer injected and actions do not duplicate this cleanup. Never print authentication tokens.
- **Windows Qt account provisioning**: `build_dependencies.ps1` preserves an existing `%APPDATA%/Qt/qtaccount.ini`
  encoded as UTF-8 with non-empty `email` and `jwt` values in `[QtAccount]`. This checks the structure and encoding,
  not server-side token validity. UTF-16 account files are not readable by Qt 6 and must be reprovisioned.
  In CI, a missing or unreadable file, or one lacking these credentials, is prepared atomically from the complete INI contents in the
  GitHub secret `QT_ACCOUNT_INI`, passed through an environment variable of the same name. The secret must contain
  actual newlines, not base64 or literal `\n`; an empty or malformed secret fails before writing the account file.
  The script logs the account file path, never credential values. CI, release and
  extended-test Windows builds use this bootstrap; `-PrepareQtAccountOnly` also prepares the recipe validation job
  without changing Conan profiles, remotes or packages. Never interpolate the secret into shell commands or log its contents.
- **macOS universal**: Only Release mode produces universal binaries; Debug compiles for current arch only
- **Local remote**: The script auto-registers a `localrecipes` remote pointing to `recipes/`. If the URL changes, it is recreated
- **Qt recipe CI**: `.github/workflows/qt-recipe-ci.yml` validates recipe changes on the Windows, macOS and Linux build
  runners through `validate_qt_recipe.py`. It keeps the runner's Conan configuration and profiles, creates Qt under
  `qt/<version>@ci/run-<run_id>-<attempt>`, and removes only that exact reference afterwards. Ordinary `qt/<version>`
  cache entries are not refreshed or removed. The workflow performs real installer/package builds, not Python unit tests.
- **Poco components**: 22+ components with recursive dependency tree. Enable/disable via `enable_*` options
- **openssl-macos vs openssl**: macOS uses `openssl-macos` (universal), other platforms use `openssl` from Conan Center

## Pre-PR Checks

```bash
# Verify recipe syntax
cd recipes && for r in */all/conanfile.py; do python3 -c "import ast; ast.parse(open('$r').read())" && echo "OK: $r"; done
# Verify config.yml validity
python3 -c "import yaml; [yaml.safe_load(open(f)) for f in __import__('glob').glob('recipes/*/config.yml')]"
```
