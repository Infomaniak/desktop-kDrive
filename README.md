# Infomaniak kDrive app

[![Extended tests - All OS](https://github.com/Infomaniak/desktop-kDrive/actions/workflows/build-and-run-extended-tests.yml/badge.svg)](https://github.com/Infomaniak/desktop-kDrive/actions/workflows/build-and-run-extended-tests.yml)

## The Desktop application for [kDrive by Infomaniak](https://www.infomaniak.com/kdrive)

### Synchronise, share, collaborate. The Swiss cloud that’s 100% secure.

#### :cloud: All the space you need

Always have access to all your photos, videos and documents.

#### :globe_with_meridians: A collaborative ecosystem. Everything included.

Collaborate online on Office documents, organise meetings, share your work. Anything is possible!

#### :lock: kDrive respects your privacy

Protect your data in a sovereign cloud exclusively developed and hosted in Switzerland. Infomaniak doesn’t analyze or
resell your data.

### [Download the kDrive app here](https://www.infomaniak.com/en/apps/download-kdrive)

Alternatively, retrieve the latest
platform-specific [direct download URLs](https://www.infomaniak.com/drive/latest).

## License & Contributions

This project is under GPLv3 license.  
If you see a bug or an enhanceable point, feel free to create an issue, so we can discuss it. Once approved, we or you
(depending on the criticality of the bug or improvement) can take care of it and open a pull request.
Please do not open a pull request before creating an issue.

## Tech things

**kDrive Desktop** started as an [Owncloud](https://owncloud.com/) fork in 2019, up until 2023 after all the core
functionalities were rewritten.
**LiteSync** is an extension for **Windows** and **macOS** providing on-demand file downloading to save space on your
device.

The **kDrive Desktop** application follows the [Syncpal Algorithm by Marius Shekow](https://hal.science/hal-02319573/).

### Languages

The project is developed in **C++** using **Qt**.  
The **macOS** extension is made in **Objective-C/C++**.

### Minimum Requirements

| System  | With LiteSync    | Without LiteSync                            | ARM
|---------|------------------|---------------------------------------------|--------------------|
| Linux   | :x:              | Ubuntu 22.04 (amd64) / Ubuntu 24.04 (arm64) | :heavy_check_mark: |
| macOS   | macOS 10.15      | macOS 10.15                                 | :heavy_check_mark: |
| Windows | Windows 10 1709  | Windows 10                                  | :x:                |

### Libraries

The **kDrive Desktop** application is using the following libraries:

- The user interface and application framework are built with [Qt](https://www.qt.io/).
- The network communications are made using [Poco](https://pocoproject.org/) libraries.
- [OpenSSL](https://openssl-library.org/) is used for cryptographic and TLS features.
- [zlib](https://zlib.net/) is used for compression support.
- [SQLite](https://www.sqlite.org/) is used for local data storage.
- [xxHash](https://xxhash.com/) is a fast Hash algorithm.
- We are monitoring the application behaviour with [Sentry](https://sentry.io/).
- [log4cplus](https://github.com/log4cplus/log4cplus) is the logging API to create and populate the log files for the
  application.
- Our Unit-Testing is done with the [CppUnit](https://www.freedesktop.org/wiki/Software/cppunit/) framework module.
- We use [libzip](https://libzip.org/) to create log archives for our support team.

## Development

The project uses **Conan 2.x** to manage its C++ dependencies. [Key libraries](#libraries)
are installed via custom [Conan](https://conan.io/) recipes located
in [`infomaniak-build-tools/conan/recipes`](./infomaniak-build-tools/conan/recipes).

The build and the release setup are documented per platform in `infomaniak-build-tools`.
Start with the guide that matches your development environment:

| [Linux](./infomaniak-build-tools/linux/Readme.md) | [macOS](./infomaniak-build-tools/macos/Readme.md) | [Windows](./infomaniak-build-tools/windows/Readme.md) |
|---------------------------------------------------|---------------------------------------------------|-------------------------------------------------------|
