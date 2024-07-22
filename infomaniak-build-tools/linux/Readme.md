# kDrive Desktop Configuration - Linux

- [kDrive files](#kdrive-files)
- [Installation Requirements](#installation-requirements)
    - [aPckages](#packages)
    - [Qt 6.2.3](#qt-623)
    - [log4cplus](#log4cplus)
    - [OpenSSL](#openssl)
    - [Poco](#poco)
    - [CPPUnit](#cppunit)
    - [Sentry](#sentry)
    - [xxHash](#xxhash)
    - [libzip](#libzip)
- [Build in Debug](#build-in-debug)
    - [Using CLion](#using-clion)
        - [Prerequisites](#prerequisites)
        - [CMake Parameters](#cmake-parameters)
    - [Qt Creator](#using-qt-creator)
        - [Configuration](#configuration)
        - [Debugging](#debugging)
- [Build in Release](#build-in-release)
    - [Podman Image](#podman-image)
    - [Building](#building)

# kDrive files

The directory `~/Projects` will be used for the installation in this documentation.  
If you wish to have the sources elsewhere, feel free to use the path you want.

```bash
cd ~/Projects
git clone https://github.com/Infomaniak/desktop-kDrive.git
cd desktop-kDrive && git submodule update --init --recursive
```

# Installation Requirements

These requirements will only apply if you want to build from a Linux device  
Currently, the build for Linux release is created from a podman container

You will need cmake and clang to compile the libraries and the kDrive project  
This documentation was made for Ubuntu 22.04 LTS

## Packages :

If not already installed, you will need **git**, **cmake** and **clang** (clang-18 or higher) packages.

```bash
sudo apt install -y git
sudo apt install -y cmake
sudo apt install -y clang
```

Ensure that CLANG is installed with at least version 18 by running the following command :

```bash
clang --version
```

If the version is lower than 18, you can install the latest version with the following commands :

```bash
wget https://apt.llvm.org/llvm.sh
chmod u+x llvm.sh
apt install lsb-release wget software-properties-common gnupg
sudo ./llvm.sh 18
sudo update-alternatives --install /usr/bin/clang clang /usr/lib/llvm-18/bin/clang 100
sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/lib/llvm-18/bin/clang++ 100
```

Check the version again with `clang --version` to ensure that the version is now 18 or higher.

## Qt 6.2.3

From the [Qt Installer](https://www.qt.io/download-qt-installer-oss?hsCtaTracking=99d9dd4f-5681-48d2-b096-470725510d34%7C074ddad0-fdef-4e53-8aa8-5e8a876d6ab4), tick the **Archive** box to see earlier Qt versions.  
In QT 6.2.3, select :
- Desktop gcc 64-bits
- Qt 5 Compatibility Module

In Qt 6.2.3 Additional Libraries, select :
- Qt WebEngine
- Qt Positioning
- Qt WebChannel
- Qt WebView

If, following the installation, you cannot load the Qt platform plugin xcb, you can run the following command :
```bash
sudo apt install libxcb-cursor0
```

## log4cplus

```bash
cd ~/Projects
git clone --recurse-submodules https://github.com/log4cplus/log4cplus.git
cd log4cplus
git checkout 2.1.x
mkdir cmake-build
cd cmake-build
cmake .. -DUNICODE=1
sudo cmake --build . --target install
```

If an error occurs with the the include of `catch.hpp`, you need to change branch inside the `catch` directory:

```bash
cd ../catch
git checkout v2.x
```

## OpenSSL

The OpenSSL Configure will require Perl to be installed first

```bash
cd ~/Projects
git clone https://github.com/openssl/openssl.git
cd openssl
git checkout tags/openssl-3.2.1
./Configure shared
make
sudo make install
```

## Poco

> :warning: **`Poco` requires [OpenSSL](#openssl) to be installed.**

For ARM64:
```bash
cd ~/Projects
git clone https://github.com/pocoproject/poco.git
cd poco
git checkout tags/poco-1.13.3-release
mkdir cmake-build
cd cmake-build
cmake .. -DOPENSSL_ROOT_DIR=/usr/local -DOPENSSL_INCLUDE_DIR=/usr/local/include -DOPENSSL_CRYPTO_LIBRARY=/usr/local/lib/libcrypto.so -DOPENSSL_SSL_LIBRARY=/usr/local/lib/libssl.so
sudo cmake --build . --target install
```

For AMD64:
```bash
cd ~/Projects
git clone https://github.com/pocoproject/poco.git
cd poco
git checkout tags/poco-1.13.3-release
mkdir cmake-build
cd cmake-build
cmake .. -DOPENSSL_ROOT_DIR=/usr/local -DOPENSSL_INCLUDE_DIR=/usr/local/include -DOPENSSL_CRYPTO_LIBRARY=/usr/local/lib64/libcrypto.so -DOPENSSL_SSL_LIBRARY=/usr/local/lib64/libssl.so
sudo cmake --build . --target install
```

## CPPUnit

```bash
sudo apt-get install -y autotools-dev
sudo apt-get install -y automake
sudo apt-get install -y libtool m4 automake
cd ~/Projects
git clone git://anongit.freedesktop.org/git/libreoffice/cppunit
cd cppunit
./autogen.sh
./configure
make
sudo make install
```

## Sentry

You will need to install the dev libcurl package to build sentry-native

```bash
sudo apt install -y libcurl4-openssl-dev
cd ~/Projects
git clone https://github.com/getsentry/sentry-native.git
cd sentry-native
git checkout tags/0.6.7
git submodule init
git submodule update --recursive
cmake -B build -DSENTRY_INTEGRATION_QT=YES -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH=~/Qt/6.7.2/gcc_arm64
cmake --build build --parallel
sudo cmake --install build
```

## xxHash

```bash
cd ~/Projects
git clone https://github.com/Cyan4973/xxHash.git
cd xxHash
git checkout tags/v0.8.2
cd cmake_unofficial
mkdir build
cd build
cmake ..
sudo cmake --build . --target install
```

## libzip

First, install the `dev` version of zlib:

```bash
sudo apt-get install zlib1g-dev
```

Clone and install libzip

```bash
cd ~/Projects
git clone https://github.com/nih-at/libzip.git
cd libzip
git checkout tags/v1.10.1
mkdir build && cd build
cmake ..
make
sudo make install
```

# Build in Debug

## Linking dependencies

In order for CMake to be able to find all dependencies, you might need to define `LD_LIBRARY_PATH=/usr/local/lib` in your environment variables.

## Using CLion

### Prerequisites

Install the following libraries:

```bash
sudo apt-get install mesa-utils
sudo apt-get install freeglut3-dev
sudo apt-get install libsqlite3-dev
sudo apt-get install -y pkg-config
sudo apt-get install libglib2.0-dev
sudo apt install libsecret-1-0 libsecret-1-dev libglib2.0-dev
```

## CMake Parameters

CMake options:

```
-DCMAKE_BUILD_TYPE:STRING=Debug
-DAPPLICATION_CLIENT_EXECUTABLE=kdrive_client
-DKDRIVE_THEME_DIR=/home/<user>/Projects/desktop-kDrive/infomaniak
-DCMAKE_INSTALL_PREFIX=/home/<user>/Projects/CLion-build-debug/bin
-DBUILD_UNIT_TESTS:BOOL=ON
-DCMAKE_PREFIX_PATH:STRING=/home/<user>/Qt/6.7.2/gcc_arm64
-DSOCKETAPI_TEAM_IDENTIFIER_PREFIX:STRING=864VDCS2QY
-DQT_DEBUG_FIND_PACKAGE=ON
```

## Using Qt Creator 

### Configuration

Open the kDrive project in your IDE   
In the project build settings, paste the following lines in the Initial Configuration Batch Edit (replace `<user>`)

```
-GUnix Makefiles
-DCMAKE_BUILD_TYPE:STRING=Debug
-DCMAKE_PROJECT_INCLUDE_BEFORE:PATH=%{IDE:ResourcePath}/package-manager/auto-setup.cmake
-DQT_QMAKE_EXECUTABLE:STRING=%{Qt:qmakeExecutable}
-DCMAKE_PREFIX_PATH:STRING=%{Qt:QT_INSTALL_PREFIX}
-DCMAKE_C_COMPILER:STRING=%{Compiler:Executable:C}
-DCMAKE_CXX_COMPILER:STRING=%{Compiler:Executable:Cxx}
-DAPPLICATION_CLIENT_EXECUTABLE=kdrive
-DKDRIVE_THEME_DIR=/home/<user>/Projects/desktop-kDrive/infomaniak
-DCMAKE_INSTALL_PREFIX=/home/<user>/Projects/build-desktop-kDrive-Desktop_Qt_6_2_3_GCC_64bit-Debug/bin
-DBUILD_TESTING=OFF
```

### Debugging

The configuration and database files are stored in the `~/.config/kDrive` directory.  
The log files will be generated in the `/tmp/kDrive-logdir` directory.

# Build in Release

Currently, the release appImage file is generated in a podman container.
For this part, please replace `[arch]` by either `amd64` or `arm64` depending on your architecture.

## Podman image

You will need Podman installed for this step, as currently our building script runs through a podman container.

To pull the podman image from our github, run :
```bash
podman pull --arch [arch] ghcr.io/infomaniak/kdrive-desktop-linux:latest
```

## Building

You can start a Linux build with the script located in `infomaniak-build-tools/linux/build-drive-[arch].sh`  
The script will start a podman machine from the image pulled using the command above, and run the appimage-build script for the chosen architecture. 

If you do not want to build through podman, you can adapt the `appimage-build-[arch].sh` script to your environment

The generated AppImage file will be located in the `build-linux-[arch]` directory.
