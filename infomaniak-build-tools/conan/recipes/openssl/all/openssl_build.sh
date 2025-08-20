#!/bin/bash

set -eo pipefail

minimum_macos_version="10.15"

log() { echo -e "[INFO] $*"; }
error() { echo -e "[ERROR] $*" >&2; exit 1; }

openssl_version=""
build_folder=""
zlib_include=""
zlib_lib=""
conan_arch=""
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
    --conan-arch)
      conan_arch="$2"
      shift 2
      ;;
    --help|-h)
      echo "Usage: $0 --version <openssl_version> --build-folder <build_folder> --zlib-include <zlib_include_path> --zlib-lib <zlib_lib_path> --conan-arch <conan_arch>"
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
[[ -z "$conan_arch" ]]      && error "The --conan-arch parameter is required."
[[ ! -d "$build_folder" || \
   ! -d "$zlib_include" || \
   ! -d "$zlib_lib" ]]      && error "The folder '$build_folder' does not exist." # Check if the folders build_folder, zlib_include, and zlib_lib exist

if ! command -v conan >/dev/null 2>&1; then
  error "Conan is not installed. Please install it first."
fi

if [[ "$conan_arch" != "armv8|x86_64" && "$conan_arch" != "x86_64|armv8" ]]; then
  error "Conan profile arch must be set to 'armv8|x86_64' or 'x86_64|armv8'. Current value: $conan_arch"
fi
echo
log "---------------------------------------"
log "OpenSSL version: $openssl_version"
log "Build folder: $build_folder"
log
log "Zlib:"
log "\t- Include path:\t$zlib_include"
log "\t- Lib path:\t$zlib_lib"
log "---------------------------------------"
echo


pushd "$build_folder"

# -----------------------------------------------------------------------------
# Download and extract OpenSSL sources
# -----------------------------------------------------------------------------

openssl_git_tag="openssl-${openssl_version}"
base_url="https://github.com/openssl/openssl/releases/download/${openssl_git_tag}"

archive="${openssl_git_tag}.tar.gz" # Ex: openssl-3.2.4.tar.gz
archive_sha="${archive}.sha256"     # Ex: openssl-3.2.4.tar.gz.sha256

log "Downloading OpenSSL ${openssl_version} sources (${archive} / ${archive_sha})..."
curl --silent -L -o "${archive}"     "${base_url}/${archive}"
curl --silent -L -o "${archive_sha}" "${base_url}/${archive_sha}"
log "OK"

log "Verifying archive checksum..."
sha256sum --check --status "${archive_sha}" || error "Checksum verification failed for ${archive}"
log "OK"

log "Extracting archive..."
tar -xzf "${archive}"
log "OK"

# Cleanup downloaded archive and checksum file
rm "${archive}" "${archive_sha}"

# Creating two versions of openssl, one for each architecture
log "Preparing source trees for architectures..."
mv "$openssl_git_tag" openssl.x86_64
cp -R openssl.x86_64 openssl.arm64

common_opts=(
  --release
  shared
  --prefix=/ --libdir=lib
  -mmacosx-version-min="$minimum_macos_version"
  no-apps
  no-docs
)

# Building the x86_64 version
log "Building OpenSSL for x86_64..."
pushd openssl.x86_64
./Configure darwin64-x86_64-cc "${common_opts[@]}"
make -j"$(sysctl -n hw.ncpu)"
popd

# Building the arm64 version
log "Building OpenSSL for arm64..."
pushd openssl.arm64
./Configure darwin64-arm64-cc enable-rc5 no-asm "${common_opts[@]}" zlib --with-zlib-include="$zlib_include" --with-zlib-lib="$zlib_lib"
make -j"$(sysctl -n hw.ncpu)"
popd

mkdir -p openssl.multi/lib
log "Merging shared libs with lipo..."

for lib in libssl.3.dylib libcrypto.3.dylib; do
  if [[ ! -f "openssl.x86_64/${lib}" ]]; then
    error "File openssl.x86_64/${lib} does not exist."
  fi
  if [[ ! -f "openssl.arm64/${lib}" ]]; then
    error "File openssl.arm64/${lib} does not exist."
  fi

  # Merge the libraries using lipo and create a universal binary
  lipo -create -output "openssl.multi/lib/$lib" "openssl.x86_64/$lib" "openssl.arm64/$lib"

  # Set the install name for the merged library
  install_name_tool -id "@rpath/$lib" "openssl.multi/lib/$lib"

  # We add the `@loader_path` to the rpath because the utility macdeployqt will try to find the libraries relative to the loader, and libss.3.dylib load libcrypto.3.dylib.
  # If @loader_path is not set, it will fail to find the libcrypto.3.dylib and will replace it to /usr/local/lib/libcrypto.3.dylib which could not exists on the user's system.
  install_name_tool -add_rpath "@loader_path" "openssl.multi/lib/$lib"
done

# Fixing dependencies for the merged libraries
install_name_tool -change "/lib/libcrypto.3.dylib" "@rpath/libcrypto.3.dylib" openssl.multi/lib/libssl.3.dylib

cp -R openssl.x86_64/include openssl.multi/include

popd

log "Done."
