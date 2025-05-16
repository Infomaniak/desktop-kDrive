# Old Dependencies Compilation Instructions

> **Note:** This file contains the old instructions used to compile and use the dependencies that are now managed by Conan.

<details>
<summary>xxHash 0.8.2</summary>

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
<summary>log4cplus</summary>

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