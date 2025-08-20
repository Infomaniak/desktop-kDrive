#!/bin/bash

set -eox pipefail

openssl_version="3.2.4"

openssl_git_tag="openssl-${openssl_version}"
src_url="https://github.com/openssl/openssl.git"

minimum_macos_version="10.15"

log() { echo -e "[INFO] $*"; }
error() { echo -e "[ERROR] $*" >&2; exit 1; }

build_folder=""
zlib_include=""
zlib_lib=""
openssl_version=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --version)
      openssl_version="$2"
      shift 2
      ;;
    --build-folder)
      build_folder="$2"
      shift 2
      ;;
    --zlib-include)
      zlib_include="$2"
      shift 2
      ;;
    --zlib-lib)
      zlib_lib="$2"
      shift 2
      ;;
    --help|-h)
      echo "Usage: $0 --version <openssl_version> --build-folder <build_folder> --zlib-include <zlib_include_path> --zlib-lib <zlib_lib_path>"
      exit 0
      ;;
    *)
      error "Unknown parameter : $1"
      ;;
  esac
done

[[ -z "$openssl_version" ]] && error "The --version parameter is required."
[[ -z "$build_folder" ]]    && error "The --build-folder parameter is required."
[[ -z "$zlib_include" ]]    && error "The --zlib-include parameter is required."
[[ -z "$zlib_lib" ]]        && error "The --zlib-lib parameter is required."
[[ ! -d "$build_folder" || \
   ! -d "$zlib_include" || \
   ! -d "$zlib_lib" ]]      && error "The folder '$build_folder' does not exist." # Check if the folders build_folder, zlib_include, and zlib_lib exist

if ! command -v conan >/dev/null 2>&1; then
  error "Conan is not installed. Please install it first."
fi

conan_arch_value="$(conan profile show --format json | jq -r '.host.settings.arch')"
if [[ "$conan_arch_value" != "armv8|x86_64" && "$conan_arch_value" != "x86_64|armv8" ]]; then
  error "Conan profile arch must be set to 'armv8|x86_64' or 'x86_64|armv8'. Current value: $conan_arch_value"
fi

pushd "$build_folder"

# Get the right path for zlib
source ./conanrun.sh || error "Failed to source conanrun.sh. Please ensure it exists."

log "Cloning OpenSSL sources..."
git clone --depth 1 --branch "$openssl_git_tag" "$src_url" openssl

# Creating two versions of openssl, one for each architecture
log "Preparing source trees for architectures..."
mv openssl openssl.x86_64
cp -R openssl.x86_64 openssl.arm64

# Building the x86_64 version
log "Building OpenSSL for x86_64..."
pushd openssl.x86_64
./Configure darwin64-x86_64-cc shared -mmacosx-version-min=$minimum_macos_version --prefix=/ --libdir=lib zlib --with-zlib-include="$zlib_include" --with-zlib-lib="$zlib_lib"
make -j"$(sysctl -n hw.ncpu)"
popd

# Building the arm64 version
log "Building OpenSSL for arm64..."
pushd openssl.arm64
./Configure darwin64-arm64-cc shared enable-rc5 no-asm -mmacosx-version-min=$minimum_macos_version --prefix=/ --libdir=lib zlib --with-zlib-include="$zlib_include" --with-zlib-lib="$zlib_lib"
make -j"$(sysctl -n hw.ncpu)"
popd

mkdir -p openssl.multi/lib
log "Merging shared libs with lipo..."
lipo -create -output openssl.multi/lib/libssl.3.dylib     openssl.x86_64/libssl.3.dylib     openssl.arm64/libssl.3.dylib
lipo -create -output openssl.multi/lib/libcrypto.3.dylib  openssl.x86_64/libcrypto.3.dylib  openssl.arm64/libcrypto.3.dylib

install_name_tool -id "@rpath/libssl.3.dylib" openssl.multi/lib/libssl.3.dylib
install_name_tool -id "@rpath/libcrypto.3.dylib" openssl.multi/lib/libcrypto.3.dylib

# We add the `@loader_path` to the rpath because the utility macdeployqt will try to find the libraries relative to the loader, and libss.3.dylib load libcrypto.3.dylib.
# If @loader_path is not set, it will fail to find the libcrypto.3.dylib and will replace it to /usr/local/lib/libcrypto.3.dylib which could not exists on the user's system.
install_name_tool -add_rpath "@loader_path" openssl.multi/lib/libssl.3.dylib
install_name_tool -add_rpath "@loader_path" openssl.multi/lib/libcrypto.3.dylib

# Fixing dependencies for the merged libraries
install_name_tool -change "/usr/local/lib/libcrypto.3.dylib" "@rpath/libcrypto.3.dylib" openssl.multi/lib/libssl.3.dylib
install_name_tool -change "/usr/lib/libz.1.dylib"            "@rpath/libz.1.dylib"      openssl.multi/lib/libssl.3.dylib
install_name_tool -change "/usr/lib/libz.1.dylib"            "@rpath/libz.1.dylib"      openssl.multi/lib/libcrypto.3.dylib

cp -R openssl.x86_64/include openssl.multi/include

popd

log "Done."
