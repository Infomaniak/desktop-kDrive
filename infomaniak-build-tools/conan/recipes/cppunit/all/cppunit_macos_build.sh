#!/usr/bin/env bash
set -eo pipefail

# Variables
minimum_macos_version="10.15"
build_folder=""
package_folder=""

# Utility functions
log()   { echo -e "[INFO]  $*"; }
error() { echo -e "[ERROR] $*" >&2; exit 1; }

usage() {
  cat <<EOF
Usage: $0 --build-folder <path> --package-folder <path> [--shared|--static] [--version <version>]
  --build-folder   Directory where cppunit will be cloned and built
  --package-folder Directory where cppunit will be packaged
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
    --package-folder)
      package_folder="$2"
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

if [[ -z "$build_folder" ]]; then
  error "You must specify --build-folder."
fi
if [[ -z "$package_folder" ]]; then
  error "You must specify --package-folder."
fi

[[ -n "$build_folder" ]] || { usage; }

git clone git://anongit.freedesktop.org/git/libreoffice/cppunit

pushd "cppunit" >/dev/null
git checkout "cppunit-$version"

## Explicit instantiations for CppUnit assertions to avoid linker errors for compatibility
#cat <<EOF > src/cppunit/explicit_instantiations.cpp
##include <cppunit/TestAssert.h>
##include <string>
#
#// Explicit instantiations for CppUnit assertions to avoid linker errors for compatibility
#template void CppUnit::assertEquals<int>(const int&, const int&, CppUnit::SourceLine, const std::string&);
#template void CppUnit::assertEquals<char>(const char&, const char&, CppUnit::SourceLine, const std::string&);
#template void CppUnit::assertEquals<long long>(const long long&, const long long&, CppUnit::SourceLine, const std::string&);
#template void CppUnit::assertEquals<double>(const double&, const double&, CppUnit::SourceLine, const std::string&);
#template void CppUnit::assertEquals<float>(const float&, const float&, CppUnit::SourceLine, const std::string&);
#template void CppUnit::assertEquals<std::string>(const std::string&, const std::string&, CppUnit::SourceLine, const std::string&);
#template void CppUnit::assertGreater<int>(const int&, const int&, CppUnit::SourceLine, const std::string&);
#template void CppUnit::assertGreater<long long>(const long long&, const long long&, CppUnit::SourceLine, const std::string&);
#
#template void CppUnit::assertLess<int>(const int&, const int&, CppUnit::SourceLine, const std::string&);
#template void CppUnit::assertLess<long long>(const long long&, const long long&, CppUnit::SourceLine, const std::string&);
#
#template void CppUnit::assertGreaterEqual<int>(const int&, const int&, CppUnit::SourceLine, const std::string&);
#template void CppUnit::assertGreaterEqual<long long>(const long long&, const long long&, CppUnit::SourceLine, const std::string&);
#
#template void CppUnit::assertEquals<unsigned long>(const unsigned long&, const unsigned long&, CppUnit::SourceLine, const std::string&);
#template void CppUnit::assertEquals<bool>(const bool&, const bool&, CppUnit::SourceLine, const std::string&);
#EOF
#echo 'libcppunit_la_SOURCES += explicit_instantiations.cpp' >> src/cppunit/Makefile.am

export CFLAGS="-mmacosx-version-min=${minimum_macos_version} -O0 -g"
export CXXFLAGS="${CFLAGS} -std=c++20"
export LDFLAGS="-mmacosx-version-min=${minimum_macos_version} -headerpad_max_install_names"

configure_args=""
if [[ ${shared} -eq 1 ]]; then
  configure_args+=" --enable-shared --disable-static"
else
  configure_args+=" --disable-shared --enable-static"
fi

configure_args+=" --enable-debug --disable-doxygen --disable-dot --prefix=${package_folder}"

./autogen.sh || error "autogen.sh failed"
./configure ${configure_args} || error "configure failed"
make -j"$(sysctl -n hw.ncpu)" || error "make failed"
make install || error "make install failed"

popd >/dev/null

log "Done."