# xxHash 0.8.2

## macOS

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

## Linux

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

## Windows

```
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