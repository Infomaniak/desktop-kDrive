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

function v4_copy_runtime_dependencies() (
    set -eo pipefail
    local source_lib_dir="$1"
    local app_dir="$2"
    shift 2

    local report
    report="$(LD_LIBRARY_PATH="$source_lib_dir:$app_dir/usr/lib" ldd "$@")"
    if grep -q 'not found' <<<"$report"; then
        echo "Unable to resolve the recovery updater runtime:" >&2
        grep 'not found' <<<"$report" >&2
        exit 1
    fi

    local -a runtime_libraries=()
    mapfile -t runtime_libraries < <(
        awk \
            '$2 == "=>" && $3 ~ /^\// && $3 !~ /^\/lib(64)?\// && $3 !~ /^\/usr\/lib(64)?\// {
                print $3
            }' <<<"$report" | sort -u
    )
    ((${#runtime_libraries[@]} > 0)) || {
        echo "No bundled runtime dependency found for the recovery updater" >&2
        exit 1
    }

    local library
    local library_dir
    local library_name
    local stem
    for library in "${runtime_libraries[@]}"; do
        library_dir="${library%/*}"
        library_name="${library##*/}"
        stem="${library_name%%.so*}.so"
        find "$library_dir" -maxdepth 1 \( -type f -o -type l \) -name "$stem*" \
            -exec cp -P -t "$app_dir/usr/lib" -- {} +
    done

    report="$(LD_LIBRARY_PATH="$app_dir/usr/lib" ldd "$@")"
    if grep -qE 'not found|/\.conan2/|=> /usr/local/lib' <<<"$report"; then
        echo "Recovery updater runtime is not self-contained:" >&2
        grep -E 'not found|/\.conan2/|=> /usr/local/lib' <<<"$report" >&2
        exit 1
    fi
)

function v4_create_linuxdeploy_resolver() (
    set -eo pipefail
    local app_dir="$1"
    local resolver_dir
    resolver_dir="$(mktemp -d)"

    local library
    local library_name
    while IFS= read -r -d '' library; do
        library_name="${library##*/}"
        case "$library_name" in
            libc.so*|libgcc_s.so*|libjpeg.so*|libm.so*|libpng*.so*|libstdc++.so*|libz.so*) continue ;;
        esac
        ln -sfn "$(readlink -f "$library")" "$resolver_dir/$library_name"
    done < <(find "$app_dir/usr/lib" -maxdepth 1 \( -type f -o -type l \) -print0)

    printf '%s\n' "$resolver_dir"
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

    # Keep the GLib runtime coherent. linuxdeploy's exclusion rules may leave libglib on the host while deploying GIO,
    # GObject and GModule from the build system. Mixing those releases corrupts GLib callbacks at runtime.
    cp -P /usr/lib/"$triplet"/lib{glib-2.0,gio-2.0,gobject-2.0,gmodule-2.0,ffi}.so.* usr/lib/

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
        usr/lib/{libglib-2.0.so.0,libgio-2.0.so.0,libgobject-2.0.so.0,libgmodule-2.0.so.0,libffi.so.8}
        usr/plugins/platforms/{libqxcb.so,libqwayland.so}
        usr/plugins/platforminputcontexts/{libcomposeplatforminputcontextplugin.so,libibusplatforminputcontextplugin.so}
        usr/plugins/wayland-graphics-integration-client/libqt-plugin-wayland-egl.so
        usr/plugins/wayland-shell-integration/libxdg-shell.so
        usr/plugins/xcbglintegrations/libqxcb-glx-integration.so
        usr/plugins/tls/libqopensslbackend.so
        usr/plugins/imageformats/libqsvg.so
        usr/plugins/iconengines/libqsvgicon.so
        usr/plugins/networkinformation/libqnetworkmanager.so
        usr/plugins/platformthemes/libqxdgdesktopportal.so
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
        usr/plugins/platformthemes/libqgtk3.so
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
    local linuxdeploy_help
    linuxdeploy_help="$(env -u LD_LIBRARY_PATH linuxdeploy --help 2>&1 || true)"
    grep -q -- '--deploy-deps-only' <<<"$linuxdeploy_help" || {
        echo "linuxdeploy does not support --deploy-deps-only" >&2
        exit 1
    }

    local -a deps_only=()
    local dir
    while IFS= read -r dir; do
        # The platform theme deliberately uses the host portal stack.
        [[ "$dir" == "$app_dir/usr/plugins/platformthemes" ]] && continue
        deps_only+=(--deploy-deps-only "$dir")
    done < <(find "$app_dir/usr/plugins" "$app_dir/usr/qml" "$app_dir/usr/lib/gio/modules" \
        -type f -name '*.so*' -printf '%h\n' | sort -u)

    local resolver_dir
    resolver_dir="$(v4_create_linuxdeploy_resolver "$app_dir")"
    trap 'rm -rf "$resolver_dir"' EXIT

    # Keep linuxdeploy itself on the host libpng/libz while exposing the other
    # bundled libraries to its dependency resolver.
    LD_LIBRARY_PATH="$resolver_dir" NO_STRIP=1 linuxdeploy --appdir "$app_dir" \
        -e "$app_dir/usr/bin/kDrive" \
        -d "$app_dir/usr/share/applications/kDrive.desktop" \
        "${deps_only[@]}" -v1
)

function v4_linuxdeploy_recovery_updater() (
    set -eo pipefail
    local app_dir="$1"
    local linuxdeploy_help
    linuxdeploy_help="$(env -u LD_LIBRARY_PATH linuxdeploy --help 2>&1 || true)"
    grep -q -- '--deploy-deps-only' <<<"$linuxdeploy_help" || {
        echo "linuxdeploy does not support --deploy-deps-only" >&2
        exit 1
    }

    # Keep linuxdeploy itself on the host libpng/libz while exposing the other
    # bundled libraries to its dependency resolver. Loading the AppDir's libpng
    # or libz into linuxdeploy makes the ARM64 build crash.
    local resolver_dir
    resolver_dir="$(v4_create_linuxdeploy_resolver "$app_dir")"
    trap 'rm -rf "$resolver_dir"' EXIT

    local report
    report="$(LD_LIBRARY_PATH="$resolver_dir" ldd \
        "$app_dir/usr/bin/kDriveRecoveryUpdater" \
        "$app_dir/usr/plugins/platforms/"*.so*)"
    if grep -q 'not found' <<<"$report"; then
        echo "linuxdeploy cannot resolve the prepared recovery updater runtime:" >&2
        grep 'not found' <<<"$report" >&2
        exit 1
    fi

    # The Qt plugin deploys every available plugin, including optional SQL drivers.
    # Deploy only the recovery updater and its platform plugins to avoid pulling in
    # unused drivers whose runtime dependencies might not be installed.
    LD_LIBRARY_PATH="$resolver_dir" NO_STRIP=1 linuxdeploy --appdir "$app_dir" \
        -e "$app_dir/usr/bin/kDriveRecoveryUpdater" \
        -d "$app_dir/kDriveRecoveryUpdater.desktop" \
        -i "$app_dir/kDriveRecoveryUpdater.png" \
        --deploy-deps-only "$app_dir/usr/plugins/platforms" -v1
)

function v4_verify_bundle() (
    set -eo pipefail
    export LC_ALL=C # avoid 'Shared library: ' to be translated
    local app_dir="$1"
    local failures=0
    local glibc_floor=""
    local glibcxx_floor=""
    local file
    local verify_bundled_glib=0

    if compgen -G "$app_dir/usr/lib/libglib-2.0.so*" >/dev/null ||
        compgen -G "$app_dir/usr/lib/libgio-2.0.so*" >/dev/null ||
        compgen -G "$app_dir/usr/lib/libgobject-2.0.so*" >/dev/null ||
        compgen -G "$app_dir/usr/lib/libgmodule-2.0.so*" >/dev/null; then
        verify_bundled_glib=1
    fi

    while IFS= read -r -d '' file; do
        readelf -h "$file" >/dev/null 2>&1 || continue

        if readelf -d "$file" | sed -n 's/.*R\(UN\)\{0,1\}PATH.*\[\(.*\)\]/\2/p' | tr ':' '\n' | grep -q '^/'; then
            echo "Absolute RUNPATH: $file" >&2
            failures=1
        fi
        if readelf -d "$file" | grep -q 'Shared library: \[libOpenGL\.so\.0\]'; then
            echo "Unsupported host dependency on libOpenGL.so.0: $file" >&2
            failures=1
        fi

        local report
        report="$(env -u LD_LIBRARY_PATH ldd "$file" 2>/dev/null || true)"
        if grep -q 'not found' <<<"$report"; then
            echo "Unresolved dependency: $file" >&2
            grep 'not found' <<<"$report" >&2
            failures=1
        fi
        if grep -qE '\.conan2/|=> /(usr/)?lib[^ ]*/libQt6' <<<"$report"; then
            echo "Library resolved outside the AppDir: $file" >&2
            grep -E '\.conan2/|=> /(usr/)?lib[^ ]*/libQt6' <<<"$report" >&2
            failures=1
        fi
        if ((verify_bundled_glib)) &&
            grep -qE '=> /(usr/)?lib[^ ]*/lib(glib-2\.0|gio-2\.0|gobject-2\.0|gmodule-2\.0)\.so' <<<"$report"; then
            echo "GLib runtime resolved outside the AppDir: $file" >&2
            grep -E '=> /(usr/)?lib[^ ]*/lib(glib-2\.0|gio-2\.0|gobject-2\.0|gmodule-2\.0)\.so' <<<"$report" >&2
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
    env -u LD_LIBRARY_PATH linuxdeploy --list-plugins 2>&1 | grep -qw appimage || {
        echo "linuxdeploy-plugin-appimage is not available" >&2
        exit 1
    }

    env -u LD_LIBRARY_PATH NO_STRIP=1 linuxdeploy --appdir "$1" --output appimage -v1
)
