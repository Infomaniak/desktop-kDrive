#!/usr/bin/env bash
set -euo pipefail

QT_VERSION="6.2.3"

echo "Installing Qt $QT_VERSION with Qt Online Installer..."

if [[ -z "$QT_EMAIL" ]]; then
  echo "ERROR: QT_EMAIL environment variable is not set." >&2
  exit 1
fi
if [[ -z "$QT_PASSWORD" ]]; then
  echo "ERROR: QT_PASSWORD environment variable is not set." >&2
  exit 1
fi
if [[ -z "$QT_INSTALL_DIRECTORY" ]]; then
  echo "ERROR: QT_INSTALL_DIRECTORY environment variable is not set." >&2
  exit 1
fi
if [[ ! -w "$QT_INSTALL_DIRECTORY" ]]; then
  echo "ERROR: No write permission on $QT_INSTALL_DIRECTORY." >&2
  exit 1
fi

TEMP_DIR=$(mktemp -d)
OLD_DIR=$(pwd)
cd "$TEMP_DIR"

function get_distant_name {
  local os=""
  local ext=""
  # Default arch is x64, because on macOS, there is no arm64 Qt Online Installer available and.
  # the x64 installer works on both x64 and arm64 macOS.
  local architecture="x64"
  if [[ "$(uname -s)" == "Darwin" ]]; then
    os="mac"
    ext="dmg"
  elif [[ "$(uname -s)" == "Linux" ]]; then
    os="linux"
    ext="run"
    if [[ "$(uname -m)" == "arm64" ]]; then
        architecture="arm64"
      fi
  fi
  echo "qt-online-installer-${os}-${architecture}-online.${ext}"
}

function get_compiler {
  if [[ "$(uname -s)" == "Darwin" ]]; then
    echo "clang_64"
  elif [[ "$(uname -s)" == "Linux" ]]; then
    echo "gcc_64"
  else
    echo "Unkonwn OS: $(uname -s)" >&2
    exit 1
  fi
}

function on_macos_detach_dmg {
  if [[ "$(uname -s)" == "Darwin" ]] && [[ -n "$MOUNT_POINT" ]]; then
    hdiutil detach "$MOUNT_POINT" || true # Detach the DMG if it is mounted. The '|| true' is to avoid script failure if the detach fails.
  fid
}

function cleanup {
  on_macos_detach_dmg
  cd "$OLD_DIR"
  rm -rf "$TEMP_DIR"
}
trap cleanup EXIT # In case of script exit, we detach the DMG on macOS and clean up the temporary directory. This include the case of errors, because of 'set -euo pipefail'.


DISTANT_NAME=$(get_distant_name)
echo "Downloading Qt Online Installer: $DISTANT_NAME"
if ! curl -fSL --progress-bar -o "$DISTANT_NAME" "https://download.qt.io/official_releases/online_installers/$DISTANT_NAME"; then
  echo "ERROR: Failed to download Qt Online Installer from https://download.qt.io/official_releases/online_installers/$DISTANT_NAME" >&2
  cleanup
  exit 1
fi

if [[ "$(uname -s)" == "Darwin" ]]; then
  # On macOS, the downloaded file is a DMG, a disk image. We have to mount it, and then find the executable inside.

  # Here, we mount the DMG file, and the grep/awk to find the mount point.
  # Exemple de sortie de hdiutil attach | grep:
  # /dev/diskxxx  Apple_HFS  /Volumes/qt-online-installer-macOS-x64-4.9.0
  MOUNT_POINT=$(
    hdiutil attach "$DISTANT_NAME" \
      -nobrowse \
      -readonly \
      | grep "/Volumes/" | awk '{print $3}'
  )

  APP_BUNDLE=$(find "$MOUNT_POINT" -type d -name "qt-online-installer-*.app" -print -quit 2>/dev/null)      # The DMG contains a macOS app Bundle
  EXEC_FOLDER="$APP_BUNDLE/Contents/MacOS/"                                                                 # The executable is inside the app bundle, in the './Contents/MacOS/' folder
  EXEC_PATH="$(find "$EXEC_FOLDER" -type f -name "qt-online-installer-macOS*" -print -quit 2>/dev/null)"    # The executable is named 'qt-online-installer-macOS-(arm|x)64-<version>', we find it in the folder
elif [[ "$(uname -s)" == "Linux" ]]; then
  # On Linux, the downloaded file is the executable, we juste need to make it executable.

  EXEC_PATH="$PWD/$DISTANT_NAME"
  chmod +x "$EXEC_PATH"
fi

if [ ! -x "$EXEC_PATH" ]; then
  echo "ERROR: Cannot find or execute '$EXEC_PATH'." >&2
  on_macos_detach_dmg
  exit 1
fi

MAJOR="$(echo "$QT_VERSION" | cut -d'.' -f1)"       # For QT_VERSION = 6.2.3, MAJOR = 6
COMPACT="$(echo "$QT_VERSION" | tr -d '.')" # For QT_VERSION = 6.2.3, COMPACT = 623


COMPILER="$(get_compiler)" # Get the compiler name based on the OS. It will be 'clang_64' on macOS and 'gcc_64' on Linux.
# Example of module: qt.qt6.623.addons.qtwebview.clang_64
"$EXEC_PATH" --email "$QT_EMAIL" --password "$QT_PASSWORD" \
  -t "$QT_INSTALL_DIRECTORY" -c --ao --al --da install \
  "qt.qt$MAJOR.$COMPACT" "qt.qt$MAJOR.$COMPACT.$COMPILER" \
    "qt.qt$MAJOR.$COMPACT.addons" \
     "qt.qt$MAJOR.$COMPACT.addons.qtpositioning" "qt.qt$MAJOR.$COMPACT.addons.qtpositioning.$COMPILER" \
     "qt.qt$MAJOR.$COMPACT.addons.qtwebchannel" "qt.qt$MAJOR.$COMPACT.addons.qtwebchannel.$COMPILER" \
     "qt.qt$MAJOR.$COMPACT.addons.qtwebengine" "qt.qt$MAJOR.$COMPACT.addons.qtwebengine.$COMPILER" \
     "qt.qt$MAJOR.$COMPACT.addons.qtwebview" "qt.qt$MAJOR.$COMPACT.addons.qtwebview.$COMPILER" \
    "qt.qt$MAJOR.$COMPACT.qt5compat" "qt.qt$MAJOR.$COMPACT.qt5compat.$COMPILER" \
    "qt.qt$MAJOR.$COMPACT.src" \
    "qt.tools" \
      "qt.tools.maintenance" \
      "qt.tools.cmake"
