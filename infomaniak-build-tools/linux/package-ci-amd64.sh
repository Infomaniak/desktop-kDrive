#!/usr/bin/env bash

#
# Infomaniak kDrive - Desktop
# Copyright (C) 2023-2025 Infomaniak Network SA
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

base_dir="$PWD"
build_dir="$base_dir/build-linux"
app_dir="$build_dir/app"

cd "$app_dir"

source "$base_dir/infomaniak-build-tools/conan/common-utils.sh"

QTDIR="$(find_qt_conan_path "$build_dir")"
export QTDIR
export QMAKE="$QTDIR/bin/qmake"
export PATH="$QTDIR/bin:$QTDIR/libexec:$PATH"
export LD_LIBRARY_PATH="$QTDIR/lib:$app_dir/usr/lib:/usr/local/lib:/usr/local/lib64:$LD_LIBRARY_PATH"
export PKG_CONFIG_PATH="$QTDIR/lib/pkgconfig:$PKG_CONFIG_PATH"

mkdir -p "$app_dir/usr/plugins"

cp -P -r "$QTDIR/plugins/"* "$app_dir/usr/plugins/"
cp -P -r "$QTDIR/libexec" "$app_dir/usr"
cp -P -r "$QTDIR/resources" "$app_dir/usr"
cp -P -r "$QTDIR/translations" "$app_dir/usr"

mv "$app_dir/usr/lib/x86_64-linux-gnu/"* "$app_dir/usr/lib/" || echo "The folder $app_dir/usr/lib/aarch64-linux-gnu/ might not exist." >&2
cp -P "$build_dir/build/conan/dependencies/*" "$app_dir/usr/lib"

mkdir -p "$app_dir/usr/qml"

rm -rf "$app_dir/usr/lib/x86_64-linux-gnu/"
rm -rf "$app_dir/usr/include/"

cp "$base_dir/sync-exclude-linux.lst" "$app_dir/usr/bin/sync-exclude.lst"
cp "$app_dir/usr/share/icons/hicolor/512x512/apps/kdrive-win.png" "$app_dir"

cp -P -r /usr/lib/x86_64-linux-gnu/nss/ "$app_dir/usr/lib/"
cp "$QTDIR/lib/libQt6SerialPort.so.6" "$app_dir/usr/lib/"

"$HOME/desktop-setup/linuxdeploy-x86_64.AppImage" --appdir "$app_dir" -e "$app_dir/usr/bin/kDrive" -i "$app_dir/kdrive-win.png" -d "$app_dir/usr/share/applications/kDrive_client.desktop" --plugin qt --output appimage -v0

version=$(grep "KDRIVE_VERSION_FULL" "$build_dir/build/version.h" | awk '{print $3}')
app_name="kDrive-$version-amd64.AppImage"

mv kDrive*.AppImage "../$app_name"

cd "$build_dir"

if [ -z ${KDRIVE_TOKEN+x} ]; then
	echo "No kDrive token found, AppImage will not be uploaded."
else
	to_upload_files=("$app_name" "kDrive-amd64.dbg" "kDrive_client-amd64.dbg")
	source "$base_dir/infomaniak-build-tools/upload_version.sh"

	for FILE in ${to_upload_files[@]}; do
		upload_file "$FILE" "linux-amd"
	done
fi
