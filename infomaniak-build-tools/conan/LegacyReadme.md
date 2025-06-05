# Old Dependencies Compilation Instructions

> **Note:** This file contains the old instructions used to compile and use the dependencies that are now managed by Conan.

<details>
<summary>xxHash - 0.8.2</summary>

### macOS
```bash
cd ~/Projects
git clone https://github.com/Cyan4973/xxHash.git
cd xxhash
git checkout tags/v0.8.2
cd cmake_unofficial
mkdir build
cd build
cmake .. -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" -DCMAKE_OSX_DEPLOYMENT_TARGET="10.15"
sudo cmake --build . --target install
```

### Linux
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

### Windows
```cmd
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
</details>

<details>
<summary>log4cplus - 2.1.0</summary>

### macOS

```bash
cd ~/Projects
git clone --recurse-submodules https://github.com/log4cplus/log4cplus.git
cd log4cplus
git checkout 2.1.x
mkdir build
cd build
cmake .. -DUNICODE=1 -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" -DCMAKE_OSX_DEPLOYMENT_TARGET="10.15"
sudo cmake --build . --target install
```

If an error occurs with the the include of `catch.hpp`, you need to change branch inside the `catch` directory:

```bash
cd ../catch
git checkout v2.x
```

### Linux

```bash
cd ~/Projects
git clone --recurse-submodules https://github.com/log4cplus/log4cplus.git
cd log4cplus
git checkout 2.1.x
cd catch
git checkout v2.x
cd ..
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

### Windows

```powershell
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

If an error occurs with the the include of `catch.hpp`, you need to change branch inside the `catch` directory:

```bash
cd ../catch
git checkout v2.x
```

</details>

<details>
<summary>OpenSSL - 3.2.4</summary>

### macOS
Download and build `OpenSSL`:

The configuration for the `x86_64` architecture is as follows:

```bash
cd ~/Projects
git clone https://github.com/openssl/openssl.git
cd openssl
git checkout tags/openssl-3.2.1
cd ..
mv openssl openssl.x86_64
cp -Rf openssl.x86_64 openssl.arm64
mkdir openssl.multi
cd openssl.x86_64
./Configure darwin64-x86_64-cc shared -mmacosx-version-min=10.15
make
```

If you have an `AMD` architecture, run `sudo make install` then continue.

The configuration for the `ARM` architecture goes as follows:

```bash
cd ~/Projects/openssl.arm64
./Configure darwin64-arm64-cc shared enable-rc5 zlib no-asm -mmacosx-version-min=10.15 
make
```

If you have an `ARM` architecture, run `sudo make install` then continue.

```bash
cd ~/Projects
lipo -arch arm64 openssl.arm64/libcrypto.3.dylib -arch x86_64 openssl.x86_64/libcrypto.3.dylib -output openssl.multi/libcrypto.3.dylib -create
lipo -arch arm64 openssl.arm64/libssl.3.dylib -arch x86_64 openssl.x86_64/libssl.3.dylib -output openssl.multi/libssl.3.dylib -create
lipo -arch arm64 openssl.arm64/libcrypto.a -arch x86_64 openssl.x86_64/libcrypto.a -output openssl.multi/libcrypto.a -create
lipo -arch arm64 openssl.arm64/libssl.a -arch x86_64 openssl.x86_64/libssl.a -output openssl.multi/libssl.a -create
sudo cp openssl.multi/* /usr/local/lib/
```

### Linux

The OpenSSL Configure will require Perl to be installed first

```bash
cd ~/Projects
git clone git@github.com:openssl/openssl.git
cd openssl
git checkout tags/openssl-3.2.1
./Configure shared
make
sudo make install
```

### Windows

Clone `OpenSSL` sources:

```powershell
cd F:\Projects
git clone git@github.com:openssl/openssl.git
cd openssl
git checkout tags/openssl-3.2.1
```

Then follow their [installation instructions](https://github.com/openssl/openssl/blob/master/NOTES-WINDOWS.md) for Windows.
Note that installing `NASM` is not required.

</details>