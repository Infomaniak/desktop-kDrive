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

version="1.15.1" # Default version, should be overridden by the version given by the recipe.
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
    --version | -v)
      version=$2
      shift 2
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
git checkout "cppunit-$version"

export CFLAGS="-mmacosx-version-min=${minimum_macos_version}"
export CXXFLAGS="${CFLAGS} -std=c++11"
export LDFLAGS="-mmacosx-version-min=${minimum_macos_version} -headerpad_max_install_names"

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
name_suffix=$([[ ${shared} -eq 1 ]] && echo "-$version" || echo "")


lib_file="cppunit/src/cppunit/.libs/libcppunit${name_suffix}.${lib_ext}"
mv "$lib_file" "lib/libcppunit.${lib_ext}"
if [[ ${shared} -eq 1 ]]; then # Correct the install name of the shared library
  install_name_tool -id "@rpath/libcppunit.dylib" "lib/libcppunit.dylib"
  log "Fixing install name of the shared library to @rpath/libcppunit.dylib"
fi

log "Done."
