# kDrive Desktop - Copilot Instructions

## Project Overview

**kDrive Desktop** is a desktop synchronization client for [kDrive by Infomaniak](https://www.infomaniak.com/kdrive), a Swiss cloud storage service. The application provides file synchronization, sharing, and collaboration features across Windows, macOS, and Linux platforms.

### Key Features
- File synchronization using the Syncpal Algorithm by Marius Shekow
- LiteSync extension for on-demand file downloading (Windows & macOS)
- Cross-platform support (Linux, macOS, Windows)
- Support for both x86_64 and ARM architectures

## Technology Stack

### Languages & Frameworks
- **C++20**: Primary language for core functionality
- **Qt**: UI framework for cross-platform desktop application
- **Swift**: macOS-specific UI and extensions (kDrive app for macOS)
- **Objective-C/C++**: macOS LiteSync extension

### Build System
- **CMake** (minimum version 3.16): Build system
- **Conan 2.x**: Dependency management (see `conanfile.py`)
- **Ninja** or **MSBuild** (Windows): Build tools

### Key Dependencies
- **Poco**: Network communications library
- **xxHash 0.8.2**: Fast hash algorithm
- **log4cplus 2.1.2**: Logging framework
- **OpenSSL 3.2.4**: Cryptography (openssl-universal on macOS)
- **zlib**: Compression library
- **CppUnit**: Unit testing framework
- **Sentry**: Application monitoring and error tracking

## Code Structure

### Directory Organization
- `src/libsyncengine/`: Core synchronization engine
- `src/libcommonserver/`: Server communication and database
- `src/libcommon/`: Common utilities and helpers
- `src/libcommongui/`: Common GUI components
- `src/gui/`: Qt-based GUI (legacy)
- `src/gui4/`: Modern Qt GUI implementation
- `src/front/macOS/`: macOS-specific Swift UI and code
- `src/server/`: Server-side components
- `test/`: Unit tests using CppUnit
- `infomaniak-build-tools/`: Build scripts for different platforms

## Coding Standards

### C++ Style
- **Standard**: C++20
- **Code Formatter**: clang-format with Google style as base (see `.clang-format`)
- **Column Limit**: 130 characters
- **Indentation**: 4 spaces (no tabs)
- **Pointer Alignment**: Right-aligned (`Type *ptr`)
- **Brace Style**: Custom (based on Google style)
- **Include Sorting**: Never auto-sort (preserve manual ordering)
- **Empty Line**: Always insert newline at EOF

### File Headers
All source files must include the GPLv3 license header:
```cpp
/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
```

### Swift Style
For macOS Swift code:
- Follow Swift standard conventions
- Use dependency injection with `@LazyInjectService` for property-level DI
- Router classes use `ObservableObject` pattern with `@Published` properties
- Use `@MainActor` for navigation methods in routers
- Include the same GPLv3 header as C++ files (with Swift comment style)

### Important Patterns

#### Dependency Injection (Swift)
- Use `@LazyInjectService` for property-level dependency injection in view controllers and view models
- Do NOT use `@InjectService` inside closures
- Example from router classes:
```swift
@LazyInjectService var queryFetcher: QueryFetcher
@LazyInjectService var coherentCache: CoherentCache
```

#### Job Classes (Swift)
- Job classes that return void may have unused `decodedMessage` variables - this is intentional
- Validation happens via `queryFetcher`
- Do not remove these seemingly unused variables

#### Router Pattern (Swift)
- Router classes use `ObservableObject` pattern with `@Published` properties for state
- Use `@MainActor` methods for navigation
- Example: `MainWindowRouter`, `MainViewRouter`

## Build & Test Commands

### Building
```bash
# Install dependencies
./infomaniak-build-tools/conan/build_dependencies.sh

# Configure with CMake
cmake -S . -B build -G Ninja

# Build
cmake --build build
```

### Testing
```bash
# Run all tests
./infomaniak-build-tools/run-tests.sh

# Run specific test
./infomaniak-build-tools/run-test.sh <test_name>
```

### Code Formatting
```bash
# Run clang-format on the codebase
./run_clang-format.sh
```

## Platform-Specific Considerations

### macOS
- Minimum version: macOS 10.15
- Universal binaries (x86_64 and arm64)
- LiteSync extension requires specific permissions
- Swift UI code in `src/front/macOS/`

### Windows
- Minimum version: Windows 10 (Windows 10 1709 for LiteSync)
- MSVC runtime: MultiThreadedDLL (/MD)
- MSI installer support

### Linux
- Minimum version: Ubuntu 22.04
- ARM support available
- No LiteSync support

## Development Workflow

### Git Practices
- Never force push
- Never use `git reset` or `git rebase` that require force push
- Work on feature branches
- Use conventional commit messages

### File Exclusions
DO NOT modify or commit:
- Build artifacts in `build/` directories
- Conan cache and generated files
- IDE-specific files (`.vscode/`, `.idea/`, etc.)
- Temporary files in `/tmp/`

### CI/CD
- GitHub Actions workflows in `.github/workflows/`
- Main CI workflow: `build-and-run-extended-tests.yml`
- Platform-specific workflows: `linux.yml`, `macos.yml`, `windows.yml`
- Release workflow: `kdrive-desktop-release.yml`

## Important Notes

### Security
- This is a GPLv3 project - respect licensing requirements
- Never commit secrets or sensitive data
- Use Sentry for error tracking (already integrated)

### Testing
- Use CppUnit for C++ unit tests
- Test templates available in `test/cppunit_test_template.cpp`
- Run tests before submitting changes

### Code Review
- All changes require review from @Infomaniak/desktop team (see CODEOWNERS)
- Follow existing patterns and conventions
- Keep changes focused and minimal

## Additional Resources

- **Algorithm Reference**: [Syncpal Algorithm by Marius Shekow](https://hal.science/hal-02319573/)
- **Project Origin**: Forked from Owncloud in 2019, fully rewritten by 2023
- **License**: GNU General Public License v3.0
- **Issue Tracker**: Create issues before merge requests

## Formatting & Style Summary

✅ **DO**:
- Use 4 spaces for indentation
- Keep lines under 130 characters
- Add GPLv3 header to all new source files
- Use right-aligned pointers (`Type *ptr`)
- Run `./run_clang-format.sh` before committing C++ code
- Follow dependency injection patterns for Swift code
- Use `@MainActor` for navigation methods in routers

❌ **DO NOT**:
- Auto-sort includes (respect manual ordering)
- Remove seemingly unused `decodedMessage` variables in Swift job methods
- Use `@InjectService` inside closures (use `@LazyInjectService` at property level)
- Commit build artifacts, dependencies, or temporary files
- Force push or rebase commits
- Modify files outside your change scope
