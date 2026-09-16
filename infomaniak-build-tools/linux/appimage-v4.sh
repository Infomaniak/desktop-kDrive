#!/usr/bin/env bash

# Post-build steps of the Linux v4 AppImage, shared by the native and container release builds.

function v4_extract_debug_symbols() (
    set -eo pipefail
    local bin_dir="$1"
    local dbg_dir="$2"
    shift 2

    for exe in "$@"; do
        env -u LD_LIBRARY_PATH objcopy --only-keep-debug "$bin_dir/$exe" "$dbg_dir/$exe.dbg"
        env -u LD_LIBRARY_PATH objcopy --strip-debug "$bin_dir/$exe"
        env -u LD_LIBRARY_PATH objcopy --add-gnu-debuglink="$dbg_dir/$exe.dbg" "$bin_dir/$exe"
    done
)

function v4_strip_unneeded_symbols() (
    set -eo pipefail
    local app_dir="$1"
    local file

    while IFS= read -r -d '' file; do
        readelf -h "$file" >/dev/null 2>&1 || continue
        readelf -SW "$file" 2>/dev/null | grep -qE '[[:space:]]\.(z?debug)_|[[:space:]]\.symtab[[:space:]]' || continue
        echo "Stripping unneeded symbols: $file"
        env -u LD_LIBRARY_PATH objcopy --strip-unneeded "$file"
    done < <(find "$app_dir/usr" -type f -print0)
)

function v4_conan_runtime_lib_path() (
    set -eo pipefail
    local conanrun
    conanrun="$(find "$1" -name conanrun.sh -print -quit)"
    [[ -n "$conanrun" ]] || {
        echo "conanrun.sh not found under '$1'" >&2
        exit 1
    }

    LD_LIBRARY_PATH=""
    source "$conanrun"
    echo "$LD_LIBRARY_PATH"
)

function v4_prepare_appdir() (
    set -eo pipefail
    cd "$1"

    # The recovery updater is packaged separately from the main AppImage.
    rm -f usr/bin/kDriveRecoveryUpdater

    # Remove development files installed by submodules such as keychain.
    rm -rf usr/include usr/lib/cmake usr/lib/pkgconfig etc
    rm -f usr/lib/*.a

    local triplet
    triplet="$(uname -m)-linux-gnu"
    if [[ -d "usr/lib/$triplet" && -n "$(ls -A "usr/lib/$triplet")" ]]; then
        echo "Unexpected multiarch library directory 'usr/lib/$triplet'" >&2
        exit 1
    fi

    # AppClientLinux points GIO_MODULE_DIR here while running from an AppImage.
    mkdir -p usr/lib/gio/modules
    if compgen -G "/usr/lib/$triplet/gio/modules/*.so" >/dev/null; then
        cp -P "/usr/lib/$triplet/gio/modules/"*.so usr/lib/gio/modules/
    fi

    cp usr/share/icons/hicolor/512x512/apps/kdrive-win.png ./kdrive-win.png
)

function v4_check_appdir() (
    set -eo pipefail
    cd "$1"

    local -a required=(
        usr/bin/{kDrive,kdrive_qml,crashpad_handler,qt.conf,sync-exclude.lst,sync-folder-rules.csv}
        usr/lib/{libQt6Core.so.6,libQt6Quick.so.6,libQt6WaylandClient.so.6,libsentry.so,libssl.so.3,libcrypto.so.3}
        usr/plugins/platforms/{libqxcb.so,libqwayland.so}
        usr/plugins/platforminputcontexts/{libcomposeplatforminputcontextplugin.so,libibusplatforminputcontextplugin.so}
        usr/plugins/wayland-graphics-integration-client/libqt-plugin-wayland-egl.so
        usr/plugins/wayland-shell-integration/libxdg-shell.so
        usr/plugins/xcbglintegrations/libqxcb-glx-integration.so
        usr/plugins/tls/libqopensslbackend.so
        usr/plugins/imageformats/libqsvg.so
        usr/plugins/iconengines/libqsvgicon.so
        usr/plugins/networkinformation/libqnetworkmanager.so
        usr/plugins/platformthemes/{libqxdgdesktopportal.so,libqgtk3.so}
        usr/qml/QtQuick/{Dialogs,VectorImage}/qmldir
        usr/qml/QtQuick/Controls/{Fusion,Basic}/qmldir
        usr/qml/Qt/labs/lottieqt/VectorImageHelpers/qmldir
        usr/translations/qt_fr.qm
        usr/share/kDrive_client/i18n/client_fr.qm
        usr/share/applications/kDrive.desktop
        kdrive-win.png
    )
    local -a forbidden=(
        usr/bin/kDriveRecoveryUpdater
        usr/plugins/qmltooling
        usr/plugins/sqldrivers
        kDriveRecoveryUpdater*.AppImage
    )
    local missing=0
    local path

    for path in "${required[@]}"; do
        [[ -e "$path" ]] || {
            echo "Missing from AppDir: $path" >&2
            missing=1
        }
    done

    for path in "${forbidden[@]}"; do
        [[ ! -e "$path" ]] || {
            echo "Unexpected in AppDir: $path" >&2
            missing=1
        }
    done

    grep -qx 'Prefix = ..' usr/bin/qt.conf || {
        echo "Unexpected qt.conf content" >&2
        missing=1
    }
    exit "$missing"
)

function v4_linuxdeploy_deploy() (
    set -eo pipefail
    local app_dir="$1"
    local extra="$2"
    local linuxdeploy_help
    linuxdeploy_help="$(linuxdeploy --help 2>&1 || true)"
    grep -q -- '--deploy-deps-only' <<<"$linuxdeploy_help" || {
        echo "linuxdeploy does not support --deploy-deps-only" >&2
        exit 1
    }

    local -a deps_only=()
    local dir
    while IFS= read -r dir; do
        # Platform themes deliberately use the host portal or GTK stack.
        [[ "$dir" == "$app_dir/usr/plugins/platformthemes" ]] && continue
        deps_only+=(--deploy-deps-only "$dir")
    done < <(find "$app_dir/usr/plugins" "$app_dir/usr/qml" "$app_dir/usr/lib/gio/modules" \
        -type f -name '*.so*' -printf '%h\n' | sort -u)

    # The icon is already installed in the AppDir. Passing it again makes the ARM64
    # linuxdeploy build crash while parsing it, before the dependency pass completes.
    NO_STRIP=1 LD_LIBRARY_PATH="$app_dir/usr/lib:$extra" linuxdeploy --appdir "$app_dir" \
        -e "$app_dir/usr/bin/kDrive" \
        -d "$app_dir/usr/share/applications/kDrive.desktop" \
        "${deps_only[@]}" -v1
)

function v4_linuxdeploy_recovery_updater() (
    set -eo pipefail
    local app_dir="$1"
    local extra="$2"
    local linuxdeploy_help
    linuxdeploy_help="$(linuxdeploy --help 2>&1 || true)"
    grep -q -- '--deploy-deps-only' <<<"$linuxdeploy_help" || {
        echo "linuxdeploy does not support --deploy-deps-only" >&2
        exit 1
    }

    # The Qt plugin deploys every available plugin, including optional SQL drivers.
    # Deploy only the recovery updater and its platform plugins to avoid pulling in
    # unused drivers whose runtime dependencies might not be installed.
    NO_STRIP=1 LD_LIBRARY_PATH="$app_dir/usr/lib:$extra" linuxdeploy --appdir "$app_dir" \
        -e "$app_dir/usr/bin/kDriveRecoveryUpdater" \
        -d "$app_dir/kDriveRecoveryUpdater.desktop" \
        -i "$app_dir/kDriveRecoveryUpdater.png" \
        --deploy-deps-only "$app_dir/usr/plugins/platforms" -v1
)

function v4_verify_bundle() (
    set -eo pipefail
    local app_dir="$1"
    local failures=0
    local glibc_floor=""
    local glibcxx_floor=""
    local file

    while IFS= read -r -d '' file; do
        readelf -h "$file" >/dev/null 2>&1 || continue

        if readelf -d "$file" | sed -n 's/.*R\(UN\)\{0,1\}PATH.*\[\(.*\)\]/\2/p' | tr ':' '\n' | grep -q '^/'; then
            echo "Absolute RUNPATH: $file" >&2
            failures=1
        fi

        local report
        report="$(env -u LD_LIBRARY_PATH ldd "$file" 2>/dev/null || true)"
        if [[ "$file" != */usr/plugins/platformthemes/libqgtk3.so ]] && grep -q 'not found' <<<"$report"; then
            echo "Unresolved dependency: $file" >&2
            grep 'not found' <<<"$report" >&2
            failures=1
        fi
        if grep -qE '\.conan2/|=> /(usr/)?lib[^ ]*/libQt6' <<<"$report"; then
            echo "Library resolved outside the AppDir: $file" >&2
            grep -E '\.conan2/|=> /(usr/)?lib[^ ]*/libQt6' <<<"$report" >&2
            failures=1
        fi

        glibc_floor="$(printf '%s\n%s\n' "$glibc_floor" "$(objdump -T "$file" 2>/dev/null | grep -oE 'GLIBC_[0-9.]+' | sort -V | tail -1)" | sort -V | tail -1)"
        glibcxx_floor="$(printf '%s\n%s\n' "$glibcxx_floor" "$(objdump -T "$file" 2>/dev/null | grep -oE 'GLIBCXX_[0-9.]+' | sort -V | tail -1)" | sort -V | tail -1)"
    done < <(find "$app_dir/usr" -type f -print0)

    echo "Bundle floor: ${glibc_floor:-none} / ${glibcxx_floor:-none}"
    exit "$failures"
)

function v4_package_appimage() (
    set -eo pipefail
    linuxdeploy --list-plugins 2>&1 | grep -qw appimage || {
        echo "linuxdeploy-plugin-appimage is not available" >&2
        exit 1
    }

    NO_STRIP=1 LD_LIBRARY_PATH="$1/usr/lib:$2" linuxdeploy --appdir "$1" --output appimage -v1
)
