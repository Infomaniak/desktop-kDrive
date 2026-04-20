#!/usr/bin/env bash

#
# Infomaniak kDrive - Desktop
# Copyright (C) 2023-2026 Infomaniak Network SA
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

# ==============================================================================
# deploy-debug.sh — Fast macOS dependency deployment for kDrive Debug builds
#
# Replaces macdeployqt for local development iterations. It copies only the
# required Qt frameworks, Conan-built dylibs, and Qt plugins into the app
# bundle, then rewires all @rpath references with install_name_tool.
#
# Usage:
#   ./infomaniak-build-tools/macos/deploy-debug.sh [APP_BUNDLE_PATH|BUNDLE_DIR]
#
# If a directory is provided, kDrive.app is resolved inside that directory.
# Defaults to the standard Debug install location when no argument is given.
# All source paths can be overridden via environment variables (see below).
# ==============================================================================
set -euo pipefail

print_usage() {
    cat <<EOF
Usage:
  ./infomaniak-build-tools/macos/deploy-debug.sh [APP_BUNDLE_PATH|BUNDLE_DIR]

Arguments:
  APP_BUNDLE_PATH   Path to kDrive.app
  BUNDLE_DIR        Directory that contains kDrive.app

Options:
  -h, --help        Show this help message and exit
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    print_usage
    exit 0
fi

if [[ "${1:-}" == -* ]]; then
    echo "ERROR: Unknown option: ${1:-}" >&2
    print_usage >&2
    exit 2
fi

# ---------------------------------------------------------------------------
# Target bundle
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
DEFAULT_APP_BUNDLE="$REPO_ROOT/build-macos/client/build/Debug/install/kDrive.app"
INPUT_PATH="${1:-}"

if [[ -z "$INPUT_PATH" ]]; then
    APP_BUNDLE="$DEFAULT_APP_BUNDLE"
elif [[ "$INPUT_PATH" == *.app ]]; then
    APP_BUNDLE="$INPUT_PATH"
else
    APP_BUNDLE="$INPUT_PATH/kDrive.app"
fi

FRAMEWORKS_DIR="$APP_BUNDLE/Contents/Frameworks"
PLUGINS_DIR="$APP_BUNDLE/Contents/PlugIns"
MACOS_DIR="$APP_BUNDLE/Contents/MacOS"
RESOURCES_DIR="$APP_BUNDLE/Contents/Resources"
EXECUTABLE="$MACOS_DIR/kDrive"

# ---------------------------------------------------------------------------
# Source paths — auto-detected from the Conan generator files in the build
# directory. Every variable can be overridden via the environment.
# ---------------------------------------------------------------------------

# Derive the Conan generators directory from the app bundle path.
# Expected layout: <build_root>/install/kDrive.app  →  <build_root>/generators/
BUILD_DIR="$(dirname "$APP_BUNDLE")"   # .../install
BUILD_DIR="$(dirname "$BUILD_DIR")"    # .../Debug  (or whatever build type)
GENERATORS_DIR="$BUILD_DIR/generators"

# Helper: extract the value of a CMake set() variable from a data file.
# Usage: conan_pkg_folder <glob-pattern-for-data-file> <variable-name>
conan_pkg_folder() {
    local pattern="$1" varname="$2"
    local file
    file=$(find "$GENERATORS_DIR" -maxdepth 1 -name "$pattern" -print -quit 2>/dev/null || true)
    if [[ -z "$file" ]]; then
        echo ""
        return
    fi
    # Extract: set(varname "/some/path")
    grep -m1 "set(${varname} " "$file" \
        | sed 's/.*set([^ ]* "\(.*\)")/\1/'
}

# Qt — pulled from CMakeCache (Qt6_DIR points into the package tree)
if [[ -z "${QT_LIB_DIR:-}" ]]; then
    _qt_cmake_dir=$(grep -m1 "^Qt6_DIR:" "$BUILD_DIR/CMakeCache.txt" 2>/dev/null \
                    | cut -d= -f2)           # …/p/lib/cmake/Qt6
    _qt_pkg_root="${_qt_cmake_dir%/lib/cmake/Qt6}"
    QT_LIB_DIR="${_qt_pkg_root}/lib"
    QT_PLUGINS_DIR="${_qt_pkg_root}/plugins"
fi
QT_LIB_DIR="${QT_LIB_DIR:?Could not detect QT_LIB_DIR — set it manually}"
QT_PLUGINS_DIR="${QT_PLUGINS_DIR:?Could not detect QT_PLUGINS_DIR — set it manually}"

# Poco
if [[ -z "${POCO_LIB_DIR:-}" ]]; then
    _poco_root=$(conan_pkg_folder "Poco-debug-*-data.cmake" "poco_PACKAGE_FOLDER_DEBUG")
    POCO_LIB_DIR="${_poco_root}/lib"
fi
POCO_LIB_DIR="${POCO_LIB_DIR:?Could not detect POCO_LIB_DIR}"

# OpenSSL (Conan package name varies per platform, e.g. openssl-macos)
if [[ -z "${OPENSSL_LIB_DIR:-}" ]]; then
    _ssl_data=$(ls "$GENERATORS_DIR"/OpenSSL-debug-*-data.cmake 2>/dev/null | head -1)
    if [[ -n "$_ssl_data" ]]; then
        # The variable name varies (e.g. openssl-macos_PACKAGE_FOLDER_DEBUG) —
        # just grab the first line that sets any *_PACKAGE_FOLDER_DEBUG variable.
        _ssl_root=$(grep -m1 'PACKAGE_FOLDER_DEBUG ' "$_ssl_data" \
                    | sed 's/.*set([^ ]* "\(.*\)")/\1/')
        OPENSSL_LIB_DIR="${_ssl_root}/lib"
    fi
fi
OPENSSL_LIB_DIR="${OPENSSL_LIB_DIR:?Could not detect OPENSSL_LIB_DIR}"

# zlib
if [[ -z "${ZLIB_LIB_DIR:-}" ]]; then
    _zlib_root=$(conan_pkg_folder "ZLIB-debug-*-data.cmake" "zlib_PACKAGE_FOLDER_DEBUG")
    ZLIB_LIB_DIR="${_zlib_root}/lib"
fi
ZLIB_LIB_DIR="${ZLIB_LIB_DIR:?Could not detect ZLIB_LIB_DIR}"

# log4cplus
if [[ -z "${LOG4CPLUS_LIB_DIR:-}" ]]; then
    _log4_root=$(conan_pkg_folder "log4cplus-debug-*-data.cmake" "log4cplus_PACKAGE_FOLDER_DEBUG")
    LOG4CPLUS_LIB_DIR="${_log4_root}/lib"
fi
LOG4CPLUS_LIB_DIR="${LOG4CPLUS_LIB_DIR:?Could not detect LOG4CPLUS_LIB_DIR}"

# xxHash
if [[ -z "${XXHASH_LIB_DIR:-}" ]]; then
    _xxhash_root=$(conan_pkg_folder "xxHash-debug-*-data.cmake" "xxhash_PACKAGE_FOLDER_DEBUG")
    XXHASH_LIB_DIR="${_xxhash_root}/lib"
fi
XXHASH_LIB_DIR="${XXHASH_LIB_DIR:?Could not detect XXHASH_LIB_DIR}"

# sentry
if [[ -z "${SENTRY_LIB_DIR:-}" ]]; then
    _sentry_root=$(conan_pkg_folder "sentry-debug-*-data.cmake" "sentry_PACKAGE_FOLDER_DEBUG")
    SENTRY_LIB_DIR="${_sentry_root}/lib"
fi
SENTRY_LIB_DIR="${SENTRY_LIB_DIR:?Could not detect SENTRY_LIB_DIR}"

# libzip — may come from Conan or Homebrew; search both
if [[ -z "${LIBZIP_PATH:-}" ]]; then
    _libzip=$(find "$BUILD_DIR" -name "libzip.*.dylib" -not -path "*.dSYM/*" 2>/dev/null | head -1)
    if [[ -z "$_libzip" ]]; then
        # Fallback: Homebrew (arm64 or x86_64)
        _libzip=$(find /opt/homebrew/lib /usr/local/lib -maxdepth 1 \
                       -name "libzip.*.dylib" 2>/dev/null | head -1)
    fi
    LIBZIP_PATH="${_libzip:-}"
fi
# libzip is optional (not all configurations need it)

# Sparkle.framework — search common locations
if [[ -z "${SPARKLE_FRAMEWORK:-}" ]]; then
    for _sp in \
        "$HOME/Library/Frameworks/Sparkle.framework" \
        "/Library/Frameworks/Sparkle.framework" \
        "/usr/local/Frameworks/Sparkle.framework"
    do
        if [[ -d "$_sp" ]]; then SPARKLE_FRAMEWORK="$_sp"; break; fi
    done
fi

# ---------------------------------------------------------------------------
# Manifest
# ---------------------------------------------------------------------------

# Resolve one dylib basename from a glob pattern in a directory.
# If several files match, pick the first one in lexical order for deterministic output.
resolve_required_dylib() {
    local lib_dir="$1" pattern="$2" label="$3"
    local matches=()
    local unique=()
    local path

    shopt -s nullglob
    for path in "$lib_dir"/$pattern; do
        [[ -e "$path" ]] || continue
        matches+=("$(basename "$path")")
    done
    shopt -u nullglob

    if [[ ${#matches[@]} -eq 0 ]]; then
        echo "ERROR: Could not find ${label} in $lib_dir (pattern: $pattern)" >&2
        exit 1
    fi

    # Deduplicate + stable order to avoid nondeterministic picks.
    while IFS= read -r path; do
        [[ -n "$path" ]] && unique+=("$path")
    done < <(printf '%s\n' "${matches[@]}" | sort -u)

    # Prefer the shortest basename (typically SONAME-style, e.g. libssl.3.dylib)
    # then lexical order for tie-breaking.
    local selected="${unique[0]}"
    for path in "${unique[@]}"; do
        if [[ ${#path} -lt ${#selected} ]]; then
            selected="$path"
        fi
    done

    if [[ ${#unique[@]} -gt 1 ]]; then
        echo "  [deploy] WARNING: Multiple matches for ${label} in $lib_dir (pattern: $pattern), using $selected" >&2
    fi

    printf '%s\n' "$selected"
}

# Qt frameworks required by the kDrive executable (includes transitive deps of the above)
QT_FRAMEWORKS=(QtCore QtDBus QtGui QtWidgets QtNetwork QtSvg QtSql)

# Poco dylibs (version auto-detected)
POCO_DYLIBS=(
    "$(resolve_required_dylib "$POCO_LIB_DIR" "libPocoFoundation.[0-9]*.dylib" "libPocoFoundation")"
    "$(resolve_required_dylib "$POCO_LIB_DIR" "libPocoCrypto.[0-9]*.dylib" "libPocoCrypto")"
    "$(resolve_required_dylib "$POCO_LIB_DIR" "libPocoNet.[0-9]*.dylib" "libPocoNet")"
    "$(resolve_required_dylib "$POCO_LIB_DIR" "libPocoNetSSL.[0-9]*.dylib" "libPocoNetSSL")"
    "$(resolve_required_dylib "$POCO_LIB_DIR" "libPocoUtil.[0-9]*.dylib" "libPocoUtil")"
    "$(resolve_required_dylib "$POCO_LIB_DIR" "libPocoXML.[0-9]*.dylib" "libPocoXML")"
    "$(resolve_required_dylib "$POCO_LIB_DIR" "libPocoJSON.[0-9]*.dylib" "libPocoJSON")"
)

# Other Conan dylibs (version auto-detected)
OPENSSL_DYLIBS=(
    "$(resolve_required_dylib "$OPENSSL_LIB_DIR" "libssl.[0-9]*.dylib" "libssl")"
    "$(resolve_required_dylib "$OPENSSL_LIB_DIR" "libcrypto.[0-9]*.dylib" "libcrypto")"
)
ZLIB_DYLIBS=("$(resolve_required_dylib "$ZLIB_LIB_DIR" "libz.[0-9]*.dylib" "libz")")
LOG4CPLUS_DYLIBS=("$(resolve_required_dylib "$LOG4CPLUS_LIB_DIR" "liblog4cplus.[0-9]*.dylib" "liblog4cplus")")
XXHASH_DYLIBS=("$(resolve_required_dylib "$XXHASH_LIB_DIR" "libxxhash.[0-9]*.dylib" "libxxhash")")
SENTRY_DYLIBS=("$(resolve_required_dylib "$SENTRY_LIB_DIR" "libsentry*.dylib" "libsentry")")

# Qt plugins: "subdir/plugin.dylib" — add more as needed
QT_PLUGINS=(
    platforms/libqcocoa.dylib
    sqldrivers/libqsqlite.dylib
    imageformats/libqgif.dylib
    imageformats/libqico.dylib
    imageformats/libqjpeg.dylib
    imageformats/libqsvg.dylib
    styles/libqmacstyle.dylib
    iconengines/libqsvgicon.dylib
    tls/libqcertonlybackend.dylib
    tls/libqsecuretransportbackend.dylib
)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

info()  { echo "  [deploy] $*"; }
warn()  { echo "  [deploy] WARNING: $*" >&2; }
step()  { echo; echo "==> $*"; }

# copy_framework <lib_dir> <FrameworkName>
copy_framework() {
    local src_dir="$1" name="$2"
    local src="$src_dir/${name}.framework"
    local dst="$FRAMEWORKS_DIR/${name}.framework"
    if [[ -d "$dst" ]]; then
        info "[skip] ${name}.framework already present"
        return
    fi
    if [[ ! -d "$src" ]]; then
        warn "${name}.framework not found at $src"
        return
    fi
    info "Copying ${name}.framework"
    cp -R "$src" "$dst"
}

# copy_dylib <full_src_path>
copy_dylib() {
    local src="$1"
    local dst="$FRAMEWORKS_DIR/$(basename "$src")"
    if [[ -e "$dst" ]]; then
        info "[skip] $(basename "$src") already present"
        return
    fi
    if [[ ! -e "$src" ]]; then
        warn "$(basename "$src") not found at $src"
        return
    fi
    info "Copying $(basename "$src")"
    cp "$src" "$dst"
    chmod 755 "$dst"
}

# add_rpath_if_missing <binary> <rpath>
add_rpath_if_missing() {
    local binary="$1" rpath="$2"
    if ! otool -l "$binary" 2>/dev/null | grep -qF "path $rpath "; then
        install_name_tool -add_rpath "$rpath" "$binary" 2>/dev/null || true
    fi
}

# fix_dylib_id <name>  — sets the install name of a dylib in Frameworks/
fix_dylib_id() {
    local name="$1"
    local path="$FRAMEWORKS_DIR/$name"
    [[ -f "$path" ]] || return
    install_name_tool -id "@rpath/$name" "$path" 2>/dev/null || true
}

# fix_binary_refs <binary>
# Rewrites any absolute Conan-cache paths embedded in the binary to @rpath.
fix_binary_refs() {
    local binary="$1"
    local current_refs
    current_refs=$(otool -L "$binary" 2>/dev/null | awk 'NR>1 {print $1}')

    local lib src_dir abs
    while IFS= read -r ref; do
        [[ "$ref" == @* ]] && continue          # already relative
        [[ "$ref" == /System/* ]] && continue   # system framework — skip
        [[ "$ref" == /usr/lib/* ]] && continue  # system dylib — skip
        [[ "$ref" == /usr/local/lib/* ]] || \
        [[ "$ref" == /Users/* ]] || true

        lib=$(basename "$ref")
        # Only rewrite if the lib landed in our Frameworks dir
        if [[ -e "$FRAMEWORKS_DIR/$lib" ]]; then
            install_name_tool -change "$ref" "@rpath/$lib" "$binary" 2>/dev/null || true
        fi
    done <<< "$current_refs"
}

# ---------------------------------------------------------------------------
# Pre-flight checks
# ---------------------------------------------------------------------------
step "Pre-flight checks"

if [[ ! -f "$EXECUTABLE" ]]; then
    echo "ERROR: kDrive executable not found at $EXECUTABLE"
    echo "       Build the project first (cmake --build ... && cmake --install ...)"
    exit 1
fi

info "Bundle  : $APP_BUNDLE"
info "Qt lib  : $QT_LIB_DIR"
info "Plugins : $QT_PLUGINS_DIR"

# ---------------------------------------------------------------------------
# Directory structure
# ---------------------------------------------------------------------------
step "Creating bundle directories"
mkdir -p "$FRAMEWORKS_DIR" "$PLUGINS_DIR" "$RESOURCES_DIR"

# ---------------------------------------------------------------------------
# Qt frameworks
# ---------------------------------------------------------------------------
step "Copying Qt frameworks"
for fw in "${QT_FRAMEWORKS[@]}"; do
    copy_framework "$QT_LIB_DIR" "$fw"
done

step "Fixing Qt framework install IDs"
for fw in "${QT_FRAMEWORKS[@]}"; do
    fw_binary="$FRAMEWORKS_DIR/${fw}.framework/Versions/A/${fw}"
    if [[ -f "$fw_binary" ]]; then
        install_name_tool -id \
            "@rpath/${fw}.framework/Versions/A/${fw}" \
            "$fw_binary" 2>/dev/null || true
    fi
done

# ---------------------------------------------------------------------------
# Third-party dylibs
# ---------------------------------------------------------------------------
step "Copying Poco dylibs"
for lib in "${POCO_DYLIBS[@]}"; do copy_dylib "$POCO_LIB_DIR/$lib"; done

step "Copying OpenSSL dylibs"
for lib in "${OPENSSL_DYLIBS[@]}"; do copy_dylib "$OPENSSL_LIB_DIR/$lib"; done

step "Copying zlib"
for lib in "${ZLIB_DYLIBS[@]}"; do copy_dylib "$ZLIB_LIB_DIR/$lib"; done

step "Copying log4cplus"
for lib in "${LOG4CPLUS_DYLIBS[@]}"; do copy_dylib "$LOG4CPLUS_LIB_DIR/$lib"; done

step "Copying xxHash"
for lib in "${XXHASH_DYLIBS[@]}"; do copy_dylib "$XXHASH_LIB_DIR/$lib"; done

step "Copying sentry"
for lib in "${SENTRY_DYLIBS[@]}"; do copy_dylib "$SENTRY_LIB_DIR/$lib"; done

step "Copying libzip"
if [[ -n "${LIBZIP_PATH:-}" ]]; then
    copy_dylib "$LIBZIP_PATH"
else
    warn "libzip not found — skipping (set LIBZIP_PATH manually if needed)"
fi

# ---------------------------------------------------------------------------
# Sparkle.framework
# ---------------------------------------------------------------------------
step "Copying Sparkle.framework"
if [[ -d "$SPARKLE_FRAMEWORK" ]]; then
    dst_sparkle="$FRAMEWORKS_DIR/Sparkle.framework"
    if [[ ! -d "$dst_sparkle" ]]; then
        info "Copying Sparkle.framework"
        cp -R "$SPARKLE_FRAMEWORK" "$dst_sparkle"
    else
        info "[skip] Sparkle.framework already present"
    fi
else
    warn "Sparkle.framework not found at $SPARKLE_FRAMEWORK — skipping"
fi

# ---------------------------------------------------------------------------
# Qt plugins
# ---------------------------------------------------------------------------
step "Copying Qt plugins"
for plugin in "${QT_PLUGINS[@]}"; do
    src="$QT_PLUGINS_DIR/$plugin"
    dst_dir="$PLUGINS_DIR/$(dirname "$plugin")"
    dst="$PLUGINS_DIR/$plugin"
    if [[ -e "$dst" ]]; then
        info "[skip] $plugin already present"
        continue
    fi
    if [[ ! -f "$src" ]]; then
        warn "$plugin not found at $src"
        continue
    fi
    info "Copying $plugin"
    mkdir -p "$dst_dir"
    cp "$src" "$dst"
    chmod 755 "$dst"
done

# ---------------------------------------------------------------------------
# Fix install IDs for all copied flat dylibs
# ---------------------------------------------------------------------------
step "Fixing dylib install IDs"
for lib in \
    "${POCO_DYLIBS[@]}" \
    "${OPENSSL_DYLIBS[@]}" \
    "${ZLIB_DYLIBS[@]}" \
    "${LOG4CPLUS_DYLIBS[@]}" \
    "${XXHASH_DYLIBS[@]}" \
    "${SENTRY_DYLIBS[@]}"
do
    fix_dylib_id "$lib"
done
fix_dylib_id "$(basename "${LIBZIP_PATH:-}")" 2>/dev/null || true

# ---------------------------------------------------------------------------
# Fix rpaths & dependency references in the main executable
# ---------------------------------------------------------------------------
step "Fixing rpaths on main executable"
add_rpath_if_missing "$EXECUTABLE" "@executable_path/../Frameworks"
fix_binary_refs "$EXECUTABLE"

# ---------------------------------------------------------------------------
# Fix rpaths & dependency references in Qt plugins
# ---------------------------------------------------------------------------
step "Fixing rpaths on Qt plugins"
for plugin in "${QT_PLUGINS[@]}"; do
    bin="$PLUGINS_DIR/$plugin"
    [[ -f "$bin" ]] || continue
    # Plugins at Contents/PlugIns/<subdir>/ need ../../Frameworks
    add_rpath_if_missing "$bin" "@loader_path/../../Frameworks"
    add_rpath_if_missing "$bin" "@executable_path/../Frameworks"
    fix_binary_refs "$bin"
done

# Fix any flat plugins already present (e.g. kDrive_vfs_mac.dylib)
for plugin_path in "$PLUGINS_DIR"/*.dylib; do
    [[ -f "$plugin_path" ]] || continue
    add_rpath_if_missing "$plugin_path" "@loader_path/../Frameworks"
    add_rpath_if_missing "$plugin_path" "@executable_path/../Frameworks"
    fix_binary_refs "$plugin_path"
done

# ---------------------------------------------------------------------------
# Fix dependency references inside copied Qt framework binaries
# (they may reference other Qt frameworks or Conan libs via absolute paths)
# ---------------------------------------------------------------------------
step "Fixing rpaths inside Qt framework binaries"
for fw in "${QT_FRAMEWORKS[@]}"; do
    fw_binary="$FRAMEWORKS_DIR/${fw}.framework/Versions/A/${fw}"
    [[ -f "$fw_binary" ]] || continue
    add_rpath_if_missing "$fw_binary" "@loader_path/../../../.."
    fix_binary_refs "$fw_binary"
done

# ---------------------------------------------------------------------------
# Write qt.conf so Qt finds its plugins inside the bundle
# ---------------------------------------------------------------------------
step "Writing qt.conf"
cat > "$RESOURCES_DIR/qt.conf" <<'EOF'
[Paths]
Plugins = PlugIns
Frameworks = Frameworks
EOF
info "qt.conf written to $RESOURCES_DIR/qt.conf"

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------
echo
echo "======================================================================"
echo " Deployment complete!"
echo " App bundle: $APP_BUNDLE"
echo " Run with : open \"$APP_BUNDLE\""
echo "======================================================================"

