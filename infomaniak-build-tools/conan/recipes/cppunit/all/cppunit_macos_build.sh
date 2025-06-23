#!/usr/bin/env bash
set -eo pipefail

# Variables
minimum_macos_version="10.15"
build_folder=""

# Utility functions
log()   { echo -e "[INFO]  $*"; }
error() { echo -e "[ERROR] $*" >&2; exit 1; }

usage() {
  cat <<EOF
Usage: $0 --build-folder <path> [--shared|--static] [--version <version>]
  --build-folder   Directory where cppunit will be cloned and built
  --shared         Build shared libraries
  --static         Build static libraries
EOF
  exit 1
}

shared=-1
static=-1

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
    --static)
      static=1
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

if [[ $static -eq 1 && $shared -eq 1 ]]; then
  error "You cannot specify both --shared and --static options."
fi

if [[ $shared -eq -1 && $static -eq -1 ]]; then
  error "You must specify either --shared or --static option."
fi

[[ -n "$build_folder" ]] || { usage; }

git clone git://anongit.freedesktop.org/git/libreoffice/cppunit

pushd "cppunit" >/dev/null
git checkout "cppunit-$version"

export CFLAGS="-mmacosx-version-min=${minimum_macos_version}"
export CXXFLAGS="${CFLAGS} -std=c++11"
export LDFLAGS="-mmacosx-version-min=${minimum_macos_version} -headerpad_max_install_names"

configure_args=""
if [[ ${shared} -eq 1 ]]; then
  configure_args+=" --enable-shared --disable-static"
else
  configure_args+=" --disable-shared --enable-static"
fi

configure_args+=" --enable-doxygen=no --enable-dot=no --enable-werror=no --enable-html-docs=no --prefix=$(pwd)/_install"

./autogen.sh || error "autogen.sh failed"
./configure ${configure_args} || error "configure failed"
make -j"$(sysctl -n hw.ncpu)" || error "make failed"
make install || error "make install failed"

popd >/dev/null
mkdir -p lib include
cp -R cppunit/_install/include/ "include/"

lib_ext=$([[ ${shared} -eq 1 ]] && echo "dylib" || echo "a")
lib_file="cppunit/_install/lib/libcppunit$([[ $shared -eq 1 ]] && echo "-$version").${lib_ext}"
mv "$lib_file" "lib/libcppunit.${lib_ext}"
if [[ ${shared} -eq 1 ]]; then # Correct the install name of the shared library
  install_name_tool -id "@rpath/libcppunit.dylib" "lib/libcppunit.dylib"
  log "Fixing install name of the shared library to @rpath/libcppunit.dylib"
fi

log "Done."