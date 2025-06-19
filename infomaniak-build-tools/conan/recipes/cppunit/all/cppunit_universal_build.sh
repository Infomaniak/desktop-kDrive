#!/usr/bin/env bash
set -eo pipefail

# Variables
src_url="http://dev-www.libreoffice.org/src/cppunit-1.15.1.tar.gz"
minimum_macos_version="10.15"
build_folder=""

# Utility functions
log()   { echo -e "[INFO]  $*"; }
error() { echo -e "[ERROR] $*" >&2; exit 1; }

usage() {
  cat <<EOF
Usage: $0 --build-folder <path> [--shared]
  --build-folder   Directory where cppunit will be cloned and built
  --shared         Build shared libraries (default is static)
EOF
  exit 1
}

shared=0

# Argument parsing
while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-folder)
      build_folder="$2"
      shift 2
      ;;
    --shared)
      shared=1
      shift 1
      ;;
    -h|--help)
      usage
      ;;
    *)
      error "Unknown parameter: $1"
      ;;
  esac
done

[[ -n "$build_folder" ]] || { usage; }

# Function to build for a given architecture
build_arch() {
  local arch="$1"
  local src_dir="cppunit.${arch}"
  local host_arg=""

  log "Preparing source for ${arch}..."
  cp -R cppunit-1.15.1 "${src_dir}" || error "mv failed for ${arch}"

  pushd "${src_dir}" >/dev/null

  export CFLAGS="-arch ${arch} -mmacosx-version-min=${minimum_macos_version}"
  export CXXFLAGS="${CFLAGS} -std=c++11"
  export LDFLAGS="-arch ${arch} -mmacosx-version-min=${minimum_macos_version}"

  [[ "${arch}" == "x86_64" ]] && host_arg="--host=x86_64-apple-darwin"

  log "Configuring for ${arch}..."
  ./autogen.sh
  if [[ ${shared} -eq 1 ]]; then
    log "CPPUnit: shared"
    host_arg="${host_arg} --enable-shared --disable-static"
  else
    log "Dep: static"
    host_arg="${host_arg} --disable-shared --enable-static"
  fi

  ./configure ${host_arg} || error "configure failed for ${arch}"

  log "Building for ${arch}..."
  make -C src/cppunit -j"$(sysctl -n hw.logicalcpu)" || error "make failed for ${arch}"

  popd >/dev/null
}

# Main script start
pushd "$build_folder" >/dev/null

log "Downloading cppunit..."
wget -q "$src_url" -O cppunit.tar.gz || error "Failed to download"
tar -xzf cppunit.tar.gz || error "Failed to extract"

for arch in x86_64 arm64; do
  build_arch "${arch}"
done
rm cppunit.tar.gz
rm -rf cppunit-1.15.1

multi_dir="cppunit.multi"
mkdir -p "${multi_dir}/lib" "${multi_dir}/include"

log "Merging libraries with lipo..."
lipo -create \
  cppunit.x86_64/src/cppunit/.libs/libcppunit-1.15.1.dylib \
  cppunit.arm64/src/cppunit/.libs/libcppunit-1.15.1.dylib \
  -output "${multi_dir}/lib/libcppunit.dylib" || error "lipo failed"
lipo -create \
  cppunit.x86_64/src/cppunit/.libs/libcppunit.a \
  cppunit.arm64/src/cppunit/.libs/libcppunit.a \
  -output "${multi_dir}/lib/libcppunit.a" || error "lipo failed"

log "Copying headers..."
cp -R cppunit.x86_64/include/cppunit "${multi_dir}/include/"

popd >/dev/null

log "Done."
