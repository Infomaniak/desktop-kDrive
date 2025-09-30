- [kDrive files](#kdrive-files)
- [Installation Requirements](#installation-requirements)
    - [Packages](#packages)
    - [Qt 6.2.3](#qt-623)
    - [Poco](#poco)
    - [CPPUnit](#cppunit)
    - [Sentry](#sentry)
    - [libzip](#libzip)
    - [Conan](#conan)
- [Build in Debug](#build-in-debug)
    - [Using CLion](#using-clion)
        - [Prerequisites](#prerequisites)
        - [CMake Parameters](#cmake-parameters)
    - [Using Qt Creator](#using-qt-creator)
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

We are migrating the dependency management from manual to using conan.

## Packages

If not already installed, you will need **git**, **cmake** and **clang** (clang-18 or higher) packages.

```bash
sudo apt update && sudo apt install -y git cmake clang
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

From the [Qt Installer](https://www.qt.io/download-qt-installer-oss?hsCtaTracking=99d9dd4f-5681-48d2-b096-470725510d34%7C074ddad0-fdef-4e53-8aa8-5e8a876d6ab4), 
tick the **Archive** box and then press the `Refresh` button to see earlier `Qt` versions.  
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

## Poco

> :warning: **`Poco` requires OpenSSL to be installed.**
>
> You **must follow** the [Conan](#conan) section first to install `OpenSSL`.

```bash
cd ~/Projects
source "$(find ./desktop-kdrive/ -name "conanrun.sh")" || exit 1 # This will prepend the path to the conan-managed dependencies to the 'LD_LIBRARY_PATH' environment variable
git clone https://github.com/pocoproject/poco.git
cd poco
git checkout tags/poco-1.13.3-release
mkdir cmake-build
cd cmake-build
cmake ..
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

If the server does not reply to the `git clone` command, you can download the source from https://www.freedesktop.org/wiki/Software/cppunit/.

You can also download cppunit version 1.15.1 using the ["Wayback Machine"](https://web.archive.org/) here: https://web.archive.org/web/20231118010938/http://dev-www.libreoffice.org/src/cppunit-1.15.1.tar.gz

## Sentry

You will need to install the dev libcurl package to build sentry-native

```bash
sudo apt install -y libcurl4-openssl-dev
cd ~/Projects
git clone https://github.com/getsentry/sentry-native.git
cd sentry-native
git checkout tags/0.7.9
git submodule init
git submodule update --recursive
cd external/crashpad
git submodule init
git submodule update --recursive
cd ../..
cmake -B build -DSENTRY_INTEGRATION_QT=YES -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH=~/Qt/6.2.3/gcc_64
cmake --build build --parallel
sudo cmake --install build
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

---

## Conan
The recommended way to install Conan is via **pip** within a Python virtual environment (Python 3.6 or newer). This approach ensures isolation and compatibility with your project’s dependencies.

> **Tip:** Other installation methods (system packages, pipx, installer scripts, etc.) are also supported. See [Conan Downloads](https://conan.io/downloads) for the full list of options.
### Prerequisites
- **Python 3.6+**
- **pip** (upgrade to the latest version):
  ```bash
  pip install --upgrade pip
  ```

### 1. Create and Activate a Virtual Environment
1. Create a virtual environment in `./.venv`:
   ```bash
   python3 -m venv .venv
   ```
2. Activate the virtual environment:
   ```bash
   source .venv/bin/activate
   ```

### 2. Install Conan

With the virtual environment activated, install Conan:
```bash
pip install conan
```

Verify the installation:
```bash
conan --version
```
You should see an output similar to:
```
Conan version 2.x.x
```

---

### 3. Configure a Conan Profile
1. Auto-generate the default profile:
   ```bash
   conan profile detect
   ```
   This creates `~/.conan2/profiles/default`.

2. Open `~/.conan2/profiles/default` and customize the settings under the `[settings]` section. For example, to target Linux with C++20:
   ```ini
    [settings]
    arch=x86_64
    build_type=Release
    compiler=gcc
    compiler.cppstd=20
    compiler.libcxx=libstdc++11
    compiler.version=11.4
    os=Linux
   ```

---

### 4. Configure CMake Toolchain Injection
The project requires additional CMake variables for a correct build. To inject these, create a file named `debug_vars.cmake` in your profiles directory (`~/.conan2/profiles`), and then reference it in the profile under `[conf]`:

1. Create or open `~/.conan2/profiles/debug_vars.cmake` and add the cache entries, for example:
   ```cmake
   set(APPLICATION_CLIENT_EXECUTABLE "kdrive_client")
   set(KDRIVE_THEME_DIR "$ENV{HOME}/Projects/desktop-kDrive/infomaniak")
   set(BUILD_UNIT_TESTS "ON")      # Set to "OFF" to skip tests
   set(CMAKE_PREFIX_PATH "$ENV{HOME}/Qt/6.7.2/gcc_arm64")
   set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/Projects/CLion-build-debug/bin")
   ```

2. In your profile (`~/.conan2/profiles/default`), add under a new `[conf]` section:
   ```ini
   [conf]
   tools.cmake.cmaketoolchain:user_toolchain+={{profile_dir}}/debug_vars.cmake
   ```
### 5. Configure the Release Profile

To **build a release version** using the script `./infomaniak-build-tools/linux/build-release-amd64.sh`, you must create a profile named `infomaniak_release`.
This profile must not contain a `tools.cmake.cmaketoolchain:user_toolchain` entry and must have the `build_type` set to `Release` or `RelWithDebInfo`.

> :information_source: This step is only required for the `build-release-amd64.sh` script, as the correct profile is already configured in the containers used by the `build-release-<arch>-via-podman.sh` scripts.
---

### 6. Install Project Dependencies

**From the repository root**, run the provided build script, specifying the desired configuration (`Debug` or `Release`) and the folder where the app will be builded.
```bash
./infomaniak-build-tools/conan/build_dependencies.sh [Debug|Release] [--output-dir=<output_dir>]
```

> **Note:** Currently only **xxHash**, **log4cplus**, **OpenSSL** and **zlib** are managed via this Conan-based workflow. Additional dependencies will be added in future updates.

---
# Build in Debug

## Linking dependencies

```bash
sudo apt-get install -y libgl1-mesa-dev \
   sqlite3 libsqlite3-dev \
   libsecret-1-dev
```
In order for CMake to be able to find all dependencies, you might need to define `LD_LIBRARY_PATH=/usr/local/lib` in your environment variables.

## Using CLion

### Prerequisites

Install the following libraries:

```bash
sudo apt update && sudo apt install -y mesa-utils freeglut3-dev \
                    libsqlite3-dev pkg-config \
                    libglib2.0-dev libsecret-1-0 \
                    libsecret-1-dev libglib2.0-dev
```


### Using Conan - CMakeUserPresets

After you followed the [Conan](#conan) section, a file named `CMakeUserPresets.json` has been created at the root of the project.
_Sometimes, CLion need to reload the CMake project to detect the presets. To do so, go, in the top bar, to `Tools` > `CMake`  and click `Reload CMake Project`._

Now you can select the `conan-[debug|release]` profile in the CMake configuration.


### Classical Way

#### CMake Parameters

CMake options:

```
-DCMAKE_BUILD_TYPE:STRING=Debug
-DAPPLICATION_CLIENT_EXECUTABLE=kdrive_client
-DKDRIVE_THEME_DIR=/home/<user>/Projects/desktop-kDrive/infomaniak
-DCMAKE_INSTALL_PREFIX=/home/<user>/Projects/CLion-build-debug/bin
-DBUILD_UNIT_TESTS:BOOL=ON
-DCMAKE_PREFIX_PATH:STRING=/home/<user>/Qt/6.7.2/gcc_arm64
-DQT_DEBUG_FIND_PACKAGE=ON
-DCMAKE_TOOLCHAIN_FILE=/home/<user>/Projects/CLion-build-debug/conan_toolchain.cmake
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

You can start a Linux build with the script located in `infomaniak-build-tools/linux/build-release-[arch].sh`  
The script will start a podman machine on Arm64 architecture from the image pulled using the command above, and run the `build-release-appimage-arm64.sh` script. 
If you do not want to build through podman, you can adapt the `build-release-appimage-arm64.sh` script to your environment.

The generated AppImage file will be located in the `build-linux-[arch]/client/install` directory.
