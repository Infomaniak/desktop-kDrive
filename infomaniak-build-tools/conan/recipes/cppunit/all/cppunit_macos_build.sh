#!/usr/bin/env bash
set -eo pipefail

# Variables
#src_url="http://dev-www.libreoffice.org/src/cppunit-1.15.2.tar.gz"
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

#wget -q "$src_url" -O cppunit.tar.gz || error "Failed to download"
#tar -xzf cppunit.tar.gz || error "Failed to extract"

git clone git://anongit.freedesktop.org/git/libreoffice/cppunit

pushd "cppunit" >/dev/null

export CFLAGS="-mmacosx-version-min=${minimum_macos_version}"
export CXXFLAGS="${CFLAGS} -std=c++11"
export LDFLAGS="-mmacosx-version-min=${minimum_macos_version}"

configure_args="" # "--build=${build_triplet} --host=${host_triplet}"
if [[ ${shared} -eq 1 ]]; then
  configure_args+=" --enable-shared --disable-static"
else
  configure_args+=" --disable-shared --enable-static"
fi

configure_args+=" --enable-doxygen=no --enable-dot=no--enable-werror=no --enable-html-docs=no"

./autogen.sh || error "autogen.sh failed"
./configure ${configure_args} || error "configure failed"
make -j"$(sysctl -n hw.ncpu)" || error "make failed"

popd >/dev/null
mkdir -p lib include
cp -R cppunit/include/cppunit "include/"

lib_ext=$([[ ${shared} -eq 1 ]] && echo "dylib" || echo "a")
name_suffix=$([[ ${shared} -eq 1 ]] && echo "-1.15.2" || echo "")


lib_file="cppunit/src/cppunit/.libs/libcppunit${name_suffix}.${lib_ext}"
mv "$lib_file" "lib/libcppunit.${lib_ext}"

log "Done."
