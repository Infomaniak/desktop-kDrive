#!/bin/bash

set -euox pipefail


log() { echo -e "[INFO] $*"; }
error() { echo -e "[ERROR] $*" >&2; exit 1; }

sentry_version=""
if [[ "$(uname)" == "Linux" ]]; then
  sentry_version="0.6.4"
else
  sentry_version="0.7.9"
fi


build_folder=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-folder)
      build_folder="$2"
      mkdir -p build_folder
      shift 2
      ;;
    *)
      error "Unknown parameter : $1"
      ;;
  esac
done

macos() {
  if ! command -v conan >/dev/null 2>&1; then
    error "Conan is not installed. Please install it first."
  fi

  if ! conan profile show 2>/dev/null | grep "arch=" | head -n 1 | cut -d'=' -f 2 | grep "armv8" | grep "x86_64" >/dev/null 2>&1; then
    error "This script should be run with a Conan profile that has multi-architecture support enabled (arch=armv8|x86_64)."
  fi

  minimum_macos_version="10.15"
  source_archive_url="https://github.com/getsentry/sentry-native/archive/refs/tags/$sentry_version.zip"

  log "Downloading sentry sources..."
  curl -L "$source_archive_url" -o "sentry-native.zip"
  unzip "sentry-native.zip"
  rm "sentry-native.zip"



}

linux() {
  echo "Not yet implemented." >&2
  exit 1
}


pushd "$build_folder"
if [[ "$(uname)" == "Darwin" ]]; then
  macos
else
  linux
fi
popd