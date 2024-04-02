# kDrive Desktop Configuration - Windows

- [kDrive files](#kdrive-files)
- [Installation Requirements](#installation-requirements)
    - [Qt 6.2.3](#qt-623)
    - [Sentry](#sentry)
    - [xxHash](#xxhash)
    - [OpenSSL](#openssl)
    - [Poco](#poco)
    - [log4cplus](#log4cplus)
    - [CPPUnit](#cppunit)
    - [Zlib](#zlib)
    - [C++ Redistributable](#redistributable)
    - [NSIS](#nsis)
    - [7za](#7za)
    - [IcoTool](#icotool)
- [Certificate Configuration](#certificate-configuration)
- [Build in Debug](#build-in-debug)
    - [Qt Creator](#using-qt-creator)
        - [Additionnal Requirements](#additionnal-requirements)
        - [CMake Parameters](#cmake-parameters)
    - [VS2019](#using-Visual-Studio-2019)
        - [Windows extension](#windows-Extension)
        - [Project setup](#project-Setup)
        - [CMake configuration](#cmake-Configuration)
        - [DLL Copy](#dll-Copy)
        - [Debugging](#debugging)
    - [Testing the extension](#testing-the-extension)
- [Build in Release](#build-in-release)
    - [Build and Packaging](#build-and-packaging)
- [Possible build errors](#possible-build-errors)

# kDrive files

The folder `F:\Projects` will be used for the installation of sources and dependencies.  
Feel free to use any directory that suits you.

```bash
mkdir F:\Projects 
cd F:\Projects
git clone https://github.com/Infomaniak/desktop-kDrive.git
cd desktop-kDrive && git submodule update --init --recursive
```

# Installation Requirements

Once `Visual Studio 2019` is installed, all commands should to be run using the `x64 Native Tools Command Prompt` with administrator permissions.  

## Visual Studio 2019

When installing `Visual Studio 2019`, select the following components:

- Desktop development with C++
- Universal Windows Platform Development
- .NET SDK
- .NET Core 3.1 Runtime
- .NET 5.0 Runtime
- .NET framework 4.7 SDK
- Windows 11 SDK (10.0.22000.0)
- Windows 10 SDK (10.0.17763.0)
- Windows 10 SDK (10.0.20348.0)

## Qt 6.2.3

From the [Qt Installer](https://www.qt.io/download-qt-installer-oss?hsCtaTracking=99d9dd4f-5681-48d2-b096-470725510d34%7C074ddad0-fdef-4e53-8aa8-5e8a876d6ab4), tick the **Archive** box to see earlier Qt versions.  
In `Qt 6.2.3`, select:
- MSVC 2019 64-bit
- Sources
- Qt 5 Compatibility Module

In `Qt 6.2.3 Additional Libraries`, select :
- Qt WebEngine
- Qt Positioning
- Qt WebChannel
- Qt WebView
- Qt Debug Information Files (only if you want to use a debugger)

In `Developer and Designer Tools` (should be selected by default):
- CMake
- Ninja

Add an environment variable named `QTDIR`, set with the path of your Qt msvc folder (which defaults to `C:\Qt\6.2.3\msvc2019_64`).
Add to the following paths to your `PATH` or adapt them to the actual location of your Qt folder if needed:
- `C:\Qt\6.2.3\msvc2019_64\bin`
- `C:\Qt\Tools\CMake_64\bin`

## Sentry

Download the [Sentry sources](https://github.com/getsentry/sentry-native/releases) and extract them to `F:\Projects`.
After successful extraction, run:

```bash
cd F:\Projects\sentry-native
cmake -B build -DSENTRY_INTEGRATION_QT=YES -DCMAKE_PREFIX_PATH=%QTDIR%
cmake --build build --config RelWithDebInfo
cmake --install build --config RelWithDebInfo
```

## xxHash

Clone and build `xxHash`:

```bash
cd F:\Projects
git clone https://github.com/Cyan4973/xxHash.git
cd xxHash
git checkout tags/v0.8.2
mkdir build
cd build
cmake ../cmake_unofficial
cmake --build .
cmake --build . --target install --config Release
```

## OpenSSL

Clone `OpenSSL` sources:

```bash
cd F:\Projects
git clone git://git.openssl.org/openssl.git
cd openssl
git checkout openssl-3.1
```

Then follow their [installation instructions](https://github.com/openssl/openssl/blob/master/NOTES-WINDOWS.md) for Windows. 
Note that installing `NASM` is not required.

## Poco

> [!WARNING] **Poco installation requires [OpenSSL](#openssl) to be installed.**

Clone and build `Poco`:

```bash
cd F:\Projects
git clone -b master https://github.com/pocoproject/poco.git
cd poco
git checkout poco-1.12.5
mkdir cmake-build
cd cmake-build
cmake -G "Visual Studio 16 2019" .. -DOPENSSL_ROOT_DIR="C:\Program Files\OpenSSL" -DOPENSSL_INCLUDE_DIR="C:\Program Files\OpenSSL\include" -DOPENSSL_CRYPTO_LIBRARY=libcrypto.lib -DOPENSSL_SSL_LIBRARY=libssl.lib
```

Open the `poco.sln` solution in Visual Studio 2019 and add `C:\Program Files\OpenSSL-Win64\lib` to the `Additional Library Directories` for the following projects:
- Crypto
- JWT
- NetSSL

While still in the `cmake-build` directory, issue the following commands:

```bash
cmake --build . --target install --config Debug
cmake --build . --target install --config Release
```

## log4cplus

Clone and build `log4cplus`:

```bash
cd F:\Projects
git clone --recurse-submodules https://github.com/log4cplus/log4cplus.git
cd log4cplus
git checkout 2.1.x
mkdir cmake-build
cd cmake-build
cmake -G "Visual Studio 16 2019" .. -DLOG4CPLUS_ENABLE_THREAD_POOL=OFF
cmake --build . --target install --config Debug
cmake --build . --target install --config Release
```

## CPPUnit

Clone `CPPUnit`:

```bash
cd F:\Projects
git clone git://anongit.freedesktop.org/git/libreoffice/cppunit
```

Then open `src/CppUnitLibrariesXXXX.sln` workspace in Visual Studio to configure as follows:
- Select all projects then right click to access `Properties`.
- Select `All configurations` and `All plateforms`,  then add `_ALLOW_ITERATOR_DEBUG_LEVEL_MISMATCH` in `C/C++ > Preprocessor > Preprocessor Definitions`.
- In the `Build` menu, select `Batch Build...`.
- Select all projects in `x64` version and click on `build`.


## Zlib

Download [Zlib](https://zlib.net/fossils/zlib-1.2.11.tar.gz) then run the following:
```bash
tar -xvzf C:\Users\%username%\Downloads\zlib-1.2.11.tar.gz -C "C:\Program Files (x86)\"
cd "C:/Program Files (x86)/zlib-1.2.11"
nmake /f win32/Makefile.msc
mkdir include
copy zconf.h include\
copy zlib.h include\
mkdir lib
copy zdll.lib lib\
copy zlib.lib lib\
copy zlib.pdb lib\
mkdir bin
copy zlib1.dll bin\
copy zlib1.pdb bin\
```

## Redistributable

Create the `F:\Projects\vcredist` folder and copy-paste the [C++ Redistributable](https://docs.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170) `x64` and `arm64` 
inside it.

## NSIS

Download and install [NSIS v3.03](https://sourceforge.net/projects/nsis/files/NSIS%203/3.03/nsis-3.03-setup.exe/download).
Add the `NSIS` path to the `PATH` environment variable.
You will need the following `NSIS` plugins:
- `LogicLib`
- `nsProcess`
- `UAC`
- `x64`

## 7za

Download [7za](https://sourceforge.net/projects/sevenzip/files/7-Zip/23.01/7z2301-extra.7z/download) and extract it in `C:\Program Files\7-Zip`. This 
requires the prior installation of `7-Zip`.

## Icoutils

Download [Icoutils](https://sourceforge.net/projects/unix-utils/files/icoutils/icoutils-0.32.3-x86_64.zip/download) and extract it.
Add the path to the `Icoutils` folder to your `PATH` environment variable, e.g. `C:\Program Files\icoutils-0.32.3-x86_64\bin`. 

# Certificate Configuration

To be able to sign executables, you need to have the Infomaniak certificate installed.  
Once installed, open `F:\Projects\desktop-kDrive\extensions\windows\cfapi\kDriveExt.sln`. Then follow the next steps:
- Select `FileExplorerExtensionPackage\Package.appxmanifest`.
- Go to the `Packaging` tab.
- Click on `Choose Certificate...` then `Select from store`.
- Copy the `AUMID` (located at the end of the Family Name, after the underscore).
- Create a new environment variable named `KDC_VIRTUAL_AUMID` with the copied `AUMID` as value.
- Repeat the same steps using the USB certificate, in an environment variable named `KDC_PHYSICAL_AUMID`.
- Update the `REGVALUE_AUMID` defined in `Vfs/cloudproviderregistrar.cpp` file if needed.

# Build in Debug

To build in `Debug` mode, you will need to build and deploy the Windows extension first.  

## Using Qt Creator
You can disable QML debugger from the settings to avoid some error pop-ups.

### Additionnal Requirements

To be able to properly debug, you will need to install the `Qt Debug Information Files` from the [`Qt 6.2.3` Section](#qt-6.2.3).
If you cannot see it, you need to tick the **Archive** box and filter again.

### CMake Parameters

Open the file `F:\Projects\desktop-kDrive\CMakeList.txt` in Qt Creator.  
Then copy the following list of `CMake` variables in "Initial CMake Parameters" using batch editing:

```bash
-GNinja
-DCMAKE_BUILD_TYPE:String=Debug
-DQT_QMAKE_EXECUTABLE:STRING=%{Qt:qmakeExecutable}
-DCMAKE_PREFIX_PATH:STRING=%{Qt:QT_INSTALL_PREFIX}
-DCMAKE_C_COMPILER:STRING=%{Compiler:Executable:C}
-DCMAKE_CXX_COMPILER:STRING=%{Compiler:Executable:Cxx}
-DAPPLICATION_UPDATE_URL:STRING=https://www.infomaniak.com/drive/update/desktopclient
-DAPPLICATION_VIRTUALFILE_SUFFIX:STRING=kdrive
-DBIN_INSTALL_DIR:PATH=F:/projects/desktop-kDrive
-DVFS_DIRECTORY:PATH=F:/Projects/desktop-kDrive/extensions/windows/cfapi/x64/Debug
-DCMAKE_EXE_LINKER_FLAGS_DEBUG:STRING=/debug /INCREMENTAL
-DCMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO:STRING=/debug /INCREMENTAL
-DCMAKE_INSTALL_PREFIX:PATH=%{ActiveProject:RunConfig:Executable:Path}
-DCRASHREPORTER_SUBMIT_URL:STRING=https://www.infomaniak.com/report/drive/crash
-DKDRIVE_THEME_DIR:STRING=F:/projects/desktop-kDrive/infomaniak
-DPLUGINDIR:STRING=C:/Program Files (x86)/kDrive/lib/kDrive/plugins
-DZLIB_INCLUDE_DIR:PATH=C:/Program Files (x86)/zlib-1.2.11/include
-DZLIB_LIBRARY_RELEASE:FILEPATH=C:/Program Files (x86)/zlib-1.2.11/lib/zlib.lib
-DWITH_CRASHREPORTER:BOOL=OFF
-DBUILD_TESTING=OFF
```

Then click "Re-configure with Initial Parameters".

## Using Visual Studio 2019
### Windows Extension

To build in Debug mode, you'll need to build and deploy the Windows extension first.

1. Open the `kDriveExt` solution located at `F:\Projects\desktop-kDrive\extensions\windows\cfapi`.
2. Navigate to the post-build events of the `Vfs` project: right click on the `Vfs` project and follow `Properties > Configuration properties > Build Events > Post-Build Event`.
3. Select `x64` in the `Platform` drop-down menu.
4. Modify `F:\Projects\` to match your actual path. The last two paths are outputs of the global projects; keep them for later steps.
5. Save and close the properties window.

Select `Debug x64` and deploy. Repeat the same steps for `Release x64`.

Close the `kDriveExt` solution.

### Project Setup

Open `Visual Studio 2019` and select `Open local folder`. Then choose `F:\Projects\desktop-kDrive`.


### CMake Configuration

1. On the configuration selector, click on "Manage configurations".
2. Create a new configuration `x64 Debug`.
3. Configure it as follows:
   - Configuration type: Debug
   - Toolset: msvc_x64_x64
   - Build root: The folder set in the post-build events of the `kDriveExt` solution.
   - CMake command args: 
    ```bash
    -DAPPLICATION_CLIENT_EXECUTABLE=kdrive_client 
    -DKDRIVE_THEME_DIR=F:/Projects/desktop-kDrive/infomaniak 
    -DWITH_CRASHREPORTER=OFF -DBUILD_UNIT_TESTS:BOOL=ON 
    -DCMAKE_PREFIX_PATH:STRING=C:/Qt/6.2.3/msvc2019_64 
    -DSOCKETAPI_TEAM_IDENTIFIER_PREFIX:STRING=864VDCS2QY 
    -DZLIB_INCLUDE_DIR:PATH="C:/Program Files (x86)/zlib-1.2.11/include" 
    -DZLIB_LIBRARY_RELEASE:FILEPATH="C:/Program Files (x86)/zlib-1.2.11/lib/zlib.lib" 
    -DVFS_STATIC_LIBRARY:FILEPATH=F:/Projects/desktop-kDrive/extensions/windows/cfapi/x64/Debug/Vfs.lib 
    -DVFS_DIRECTORY:PATH=F:/Projects/desktop-kDrive/extensions/windows/cfapi/x64/Debug 
    -DPocoCrypto_DIR:PATH="C:/Program Files (x86)/Poco/cmake" 
    -DPocoFoundation_DIR:PATH="C:/Program Files (x86)/Poco/cmake" 
    -DPocoJSON_DIR:PATH="C:/Program Files (x86)/Poco/cmake" 
    -DPocoNetSSL_DIR:PATH="C:/Program Files (x86)/Poco/cmake" 
    -DPocoNet_DIR:PATH="C:/Program Files (x86)/Poco/cmake" 
    -DPocoUtil_DIR:PATH="C:/Program Files (x86)/Poco/cmake" 
    -DPocoXML_DIR:PATH="C:/Program Files (x86)/Poco/cmake" 
    -DPoco_DIR:PATH="C:/Program Files (x86)/Poco/cmake"
    ```
   You may need to adjust paths based on your installation.
   
   - Check that `Advanced settings > install path` is the the build root path.

Save (CTRL + S). `CMake` will automatically run in the output window. 
Make sure no errors occur.

### Install

In the `Solution Explorer`, go to the available view:

![VS2019 switch view button](./doc-images/VS_2019_switch_sln_to_targets.png)

Select `CMake` targets.
Right-click on the `kDrive` executable and then on `Install`.
Once done, right-click on the `kDrive_client` executable and then on `Install`.


### DLL Copy

During the next step, you may encounter missing DLL errors. If so, copy the required DLLs into the `bin` folder of your output directory. The DLLs are located in:
- `C:\Program Files (x86)\Poco\bin`
- `C:\Program Files (x86)\log4cplus\bin`
- `C:\Program Files (x86)\NSIS\Bin`
- `C:\Program Files (x86)\zlib-1.2.11`
- `C:\Program Files\OpenSSL\bin`

### Debugging

In the `Solution Explorer`, go to the available view:
Select CMake targets.

Right-click on the `kDrive executable`: `Debug > kDrive.exe`.

Once `kDrive.exe` is running, right-click on the client executable: `Debug > kDrive_client.exe`.

Once `kDrive.exe` is running, right-click on the `kDrive_client` executable: `Debug > kDrive_client.exe`.


## Testing the extension

To test the extension in Debug mode, you will first need to install a [release version](https://www.infomaniak.com/en/apps/download-kdrive) of `kDrive`.  
Once installed and running, stop the `File Explorer` with the command:

```bash
taskkill /f /im explorer.exe
```

Then, copy the DLLs :

```bash
copy "F:\Projects\build-kdrive-Desktop_Qt_6_2_3_MSVC2019_64bit-Debug\bin\KDContextMenu.dll" "C:\Program Files (x86)\kDrive\shellext"
copy "F:\Projects\build-kdrive-Desktop_Qt_6_2_3_MSVC2019_64bit-Debug\bin\KDOverlays.dll" "C:\Program Files (x86)\kDrive\shellext"
```

Then restart the `File Explorer`:

```bash
start explorer.exe
```

# Build in Release

## Build and Packaging

The script `build-drive.ps1` will build, sign, then package the project. You can either start it from the root of the `desktop-kDrive` repository, or provide a path when executing it.    
To get more information, call the script with the option `-h` or `-help`

**Note.** For `CMake` to be able to build the project, you need to initialise the environment for `x64` with `vcvarsall.bat`, or `vcvars64.bat` (see the help output of `build-drive.ps1` for details).

```bash
cd F:\Projects\desktop-kDrive
powershell infomaniak-build-tools\windows\build-drive.ps1
```

# Possible build errors

When building in Debug mode, the following error may occur when `CMAKE_INSTALL_PREFIX` is incorrect.

![sentry_init Debug Error](./doc-images/Qt_Debug_Error.png)

The `INSTALL_PREFIX` must not end with `bin`, and if so you will need to adjust its value.
For example, `F:/Projects/build-desktop-kDrive-Desktop_Qt_6_2_3_MSVC2019_64bit-Debug/bin` must be changed to `F:/Projects/build-desktop-kDrive-Desktop_Qt_6_2_3_MSVC2019_64bit-Debug`.
