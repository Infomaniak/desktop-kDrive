#!/bin/bash

set -eox pipefail

openssl_version="3.2.4"

openssl_git_tag="openssl-${openssl_version}"
src_url="https://github.com/openssl/openssl.git"

minimum_macos_version="10.15"

log() { echo -e "[INFO] $*"; }
error() { echo -e "[ERROR] $*" >&2; exit 1; }

build_folder=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-folder)
      build_folder="$2"
      shift 2
      ;;
    *)
      error "Unknown parameter : $1"
      ;;
  esac
done

if ! command -v conan >/dev/null 2>&1; then
  error "Conan is not installed. Please install it first."
fi

if ! conan profile show 2>/dev/null | grep "arch=" | head -n 1 | cut -d'=' -f 2 | grep "armv8" | grep "x86_64" >/dev/null 2>&1; then
  error "This script should be run with a Conan profile that has multi-architecture support enabled (arch=armv8|x86_64)."
fi

pushd "$build_folder"

# Get the right path for zlib
source ./conanrun.sh || error "Failed to source conanrun.sh. Please ensure it exists and is executable."

echo $DYLD_LIBRARY_PATH

log "Cloning OpenSSL sources..."
git clone --depth 1 --branch "$openssl_git_tag" "$src_url" openssl

# Creating two versions of openssl, one for each architecture
log "Preparing source trees for architectures..."
mv openssl openssl.x86_64
cp -R openssl.x86_64 openssl.arm64

# Building the x86_64 version
log "Building OpenSSL for x86_64..."
pushd openssl.x86_64
./Configure darwin64-x86_64-cc shared -mmacosx-version-min=$minimum_macos_version
make -j"$(sysctl -n hw.ncpu)"
popd

# Building the arm64 version
log "Building OpenSSL for arm64..."
pushd openssl.arm64
./Configure darwin64-arm64-cc shared enable-rc5 zlib no-asm -mmacosx-version-min=$minimum_macos_version
make -j"$(sysctl -n hw.ncpu)"
popd

mkdir -p openssl.multi/lib
log "Merging shared libs with lipo..."
lipo -create -output openssl.multi/lib/libssl.3.dylib     openssl.x86_64/libssl.3.dylib     openssl.arm64/libssl.3.dylib
lipo -create -output openssl.multi/lib/libcrypto.3.dylib  openssl.x86_64/libcrypto.3.dylib  openssl.arm64/libcrypto.3.dylib

install_name_tool -id "@rpath/libssl.3.dylib" openssl.multi/lib/libssl.3.dylib
install_name_tool -id "@rpath/libcrypto.3.dylib" openssl.multi/lib/libcrypto.3.dylib

cp -R openssl.x86_64/include openssl.multi/include

popd

log "Done."
