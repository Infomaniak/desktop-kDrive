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

  # Host and build triplets
  local host_arch="${arch}"
  [[ "${arch}" == "arm64" ]] && host_arch="aarch64"

  local build_arch="$(uname -m)"
  [[ "${build_arch}" == "arm64" ]] && build_arch="aarch64"

  local host_triplet="${host_arch}-apple-darwin"
  local build_triplet="${build_arch}-apple-darwin"

  log "Preparing source for ${arch}..."
  cp -R cppunit-1.15.1 "${src_dir}" || error "cp failed for ${arch}"

  pushd "${src_dir}" >/dev/null

  export CFLAGS="-arch ${arch} -mmacosx-version-min=${minimum_macos_version}"
  export CXXFLAGS="${CFLAGS} -std=c++11"
  export LDFLAGS="-arch ${arch} -mmacosx-version-min=${minimum_macos_version}"

  local configure_args="--build=${build_triplet} --host=${host_triplet}"
  if [[ ${shared} -eq 1 ]]; then
    configure_args+=" --enable-shared --disable-static"
  else
    configure_args+=" --disable-shared --enable-static"
  fi

  log "Configuring and building for ${arch}..."
  ./autogen.sh                                        || error "autogen.sh failed for ${arch}"
  ./configure ${configure_args}                       || error "configure failed for ${arch}"
  make -C src/cppunit -j"$(sysctl -n hw.logicalcpu)"  || error "make failed for ${arch}"

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

# Set the library extension based on whether shared libraries are requested
lib_ext=$([[ ${shared} -eq 1 ]] && echo "dylib" || echo "a")

lib_x86="cppunit.x86_64/src/cppunit/.libs/libcppunit-1.15.1.${lib_ext}"
lib_arm="cppunit.arm64/src/cppunit/.libs/libcppunit-1.15.1.${lib_ext}"
lib_out="${multi_dir}/lib/libcppunit.${lib_ext}"

lipo -create "${lib_x86}" "${lib_arm}" -output "${lib_out}" || error "lipo failed"
lib_path="${lib_out}"

if ! lipo -info "${lib_path}" | grep -q 'x86_64.*arm64'; then # Check if the fat binary was created successfully
  error "lipo failed to create a fat binary for ${lib_ext} library"
fi

log "Copying headers..."
cp -R cppunit.x86_64/include/cppunit "${multi_dir}/include/"

popd >/dev/null

log "Done."
