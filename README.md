# Infomaniak kDrive app
[![Extended tests - All OS](https://github.com/Infomaniak/desktop-kDrive/actions/workflows/build-and-run-extended-tests.yml/badge.svg)](https://github.com/Infomaniak/desktop-kDrive/actions/workflows/build-and-run-extended-tests.yml)
<a href="https://github.com/Infomaniak/desktop-kDrive/releases"><img src="https://img.shields.io/github/v/release/Infomaniak/desktop-kDrive"></a>

## The Desktop application for [kDrive by Infomaniak](https://www.infomaniak.com/kdrive).
### Synchronise, share, collaborate.  The Swiss cloud that’s 100% secure.

#### :cloud: All the space you need
Always have access to all your photos, videos and documents. kDrive can store up to 106 TB of data.

#### :globe_with_meridians: A collaborative ecosystem. Everything included. 
Collaborate online on Office documents, organise meetings, share your work. Anything is possible!

#### :lock:  kDrive respects your privacy
Protect your data in a sovereign cloud exclusively developed and hosted in Switzerland. Infomaniak doesn’t analyze or resell your data.

### [Download the kDrive app here](https://www.infomaniak.com/en/apps/download-kdrive)

## License & Contributions
This project is under GPLv3 license.  
If you see a bug or an enhanceable point, feel free to create an issue, so that we can discuss about it, and once approved, we or you (depending on the criticality of the bug/improvement) will take care of the issue and apply a merge request.  
Please, don't do a merge request before creating an issue.

## Tech things
**kDrive Desktop** started as an [Owncloud](https://owncloud.com/) fork in 2019, up until 2023 after all the core functionalities were rewritten  
**LiteSync** is an extension for **Windows** and **macOS** providing on-demand file downloading to save space on your device  

The **kDrive Desktop** application follows the [Syncpal Algorithm by Marius Shekow](https://hal.science/hal-02319573/)

### Programming Languages
The project uses multiple programming languages, each serving specific purposes:

| Language | Usage | Primary Location |
|----------|-------|------------------|
| **C++** | Core sync engine, server communication, and business logic | `src/libsyncengine/`, `src/server/`, `src/libcommon*/` |
| **Qt/QML** | Cross-platform GUI framework and components | `src/gui/`, `src/libcommongui/` |
| **Swift** | macOS native UI | `src/front/macOS/` |
| **Objective-C/C++** | macOS extensions (Finder Sync, LiteSync) | `extensions/macos/` |
| **C#** | Windows modern UI (WinUI 3) and bootstrapper | `src/gui4/windows/` |
| **Python** | Build automation and utilities | `infomaniak-build-tools/`, translation helpers |
| **PowerShell** | Windows build scripts and automation | `infomaniak-build-tools/windows/` |
| **Shell** | Linux/macOS build scripts and automation | `infomaniak-build-tools/linux/`, `infomaniak-build-tools/macos/` |

### Minimum Requirements
| System | With LiteSync | Without LiteSync | ARM
|---|---|---|---|
| Linux | :x: | Ubuntu 22.04 | :heavy_check_mark:
| macOS | macOS 10.15 | macOS 10.15 | :heavy_check_mark:
| Windows | Windows 10 1709	| Windows 10 | :x:

### Libraries and Dependencies

#### Core C++ Dependencies
- **[Qt 6.2.3](https://www.qt.io/)** - Cross-platform GUI framework (Widgets, QML, WebEngine, WebChannel)
- **[Poco 1.13.3](https://pocoproject.org/)** - Network communications, HTTP/HTTPS client, JSON parsing
- **[OpenSSL 3.2.4](https://www.openssl.org/)** - Cryptography and secure communications
- **[xxHash 0.8.2](https://xxhash.com/)** - Fast non-cryptographic hash algorithm for file checksums
- **[log4cplus 2.1.2](https://github.com/log4cplus/log4cplus)** - Logging framework for application logs
- **[libzip 1.10.1](https://libzip.org/)** - ZIP archive creation for log packaging
- **[Sentry Native 0.7.9](https://sentry.io/)** - Application monitoring and error tracking
- **[CppUnit](https://www.freedesktop.org/wiki/Software/cppunit/)** - Unit testing framework
- **[zlib](https://zlib.net/)** - Compression library

#### C# / WinUI Dependencies (Windows Modern GUI)
- **[Microsoft.WindowsAppSDK 1.8](https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/)** - Windows App SDK for WinUI 3
- **[CommunityToolkit.Mvvm 8.4.0](https://github.com/CommunityToolkit/dotnet)** - MVVM helpers and utilities
- **[CommunityToolkit.WinUI](https://github.com/CommunityToolkit/Windows)** - WinUI controls and Lottie animations
- **[DynamicData 9.4.1](https://github.com/reactivemarbles/DynamicData)** - Reactive collections
- **[H.NotifyIcon.WinUI 2.3.1](https://github.com/HavenDV/H.NotifyIcon)** - System tray icon support
- **[Sentry 6.0.0](https://sentry.io/)** - Error tracking for .NET
- **[Serilog 4.2.0](https://serilog.net/)** - Structured logging for .NET

#### macOS Dependencies
- **[Sparkle 2.6.4](https://sparkle-project.org/)** - Auto-update framework for macOS

#### Build Tools
- **[CMake 3.16+](https://cmake.org/)** - Build system generator
- **[Conan 2.x](https://conan.io/)** - C/C++ package manager
- **[Ninja](https://ninja-build.org/)** - Build system (faster than Make)

## Building the Application

### Quick Start
```bash
# Clone repository
git clone https://github.com/Infomaniak/desktop-kDrive.git
cd desktop-kDrive
git submodule update --init --recursive

# Install dependencies (requires Conan 2.x)
./infomaniak-build-tools/conan/build_dependencies.sh Release  # Linux/macOS
./infomaniak-build-tools/conan/build_dependencies.ps1 Release # Windows

# Build
cmake -S . -B build -G Ninja
cmake --build build
```

**Prerequisites:** Git, CMake 3.16+, Qt 6.2.3, Conan 2.x, and platform-specific compilers.

### Platform-Specific Builds

#### :penguin: Linux (Ubuntu 22.04+)
Requires Clang 18+, Qt 6.2.3, Poco, Sentry, CppUnit, libzip.  
See [infomaniak-build-tools/linux/Readme.md](infomaniak-build-tools/linux/Readme.md) for detailed instructions.
```bash
./infomaniak-build-tools/linux/build-release.sh
```

#### :apple: macOS (10.15+)
Requires Xcode, Qt 6.2.3, Poco, Sentry, CppUnit, libzip, Sparkle.  
See [infomaniak-build-tools/macos/Readme.md](infomaniak-build-tools/macos/Readme.md) for detailed instructions.
```bash
./infomaniak-build-tools/macos/build-release.sh
```

#### :window: Windows (10+)
Requires Visual Studio 2019/2022+, Qt 6.2.3, Poco, Sentry, CppUnit, libzip, zlib, NSIS.  
See [infomaniak-build-tools/windows/Readme.md](infomaniak-build-tools/windows/Readme.md) for detailed instructions.
```powershell
./infomaniak-build-tools/windows/build-drive.ps1
```

### Testing
```bash
./infomaniak-build-tools/run-tests.sh
```
See [test/](test/) for more information.
