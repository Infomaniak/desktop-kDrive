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

You will need cmake and gcc-c++ to compile the libraries and the kDrive project  
This documentation was made for Ubuntu 22.04 LTS

## Packages :

If not already installed, you will need **git**, **cmake** and **clang** packages.

```bash
sudo apt install -y git
sudo apt install -y cmake
sudo apt install -y clang
```

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

## OpenSSL

The OpenSSL Configure will require Perl to be installed first

```bash
cd ~/Projects
git clone git://git.openssl.org/openssl.git
cd openssl
git checkout tags/openssl-3.2.1
./Configure shared
make
sudo make install
```

## Poco

> :warning: **`Poco` requires [OpenSSL](#openssl) to be installed.**

```bash
cd ~/Projects
git clone https://github.com/pocoproject/poco.git
cd poco
git checkout tags/poco-1.13.2-release
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
cmake -B build -DSENTRY_INTEGRATION_QT=YES -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH=~/Qt/6.2.3/gcc_64
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

Clone and install libzip

```bash
cd ~/Projects
git clone https://github.com/nih-at/libzip.git
cd libzip
git checkout tags/v1.10.1
mkdir build && cd build
cmake ..
make
make install
```

# Build in Debug

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

In order that all libraries are found, you might need to define `LD_LIBRARY_PATH=/usr/local/lib`in your environment variables.
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
