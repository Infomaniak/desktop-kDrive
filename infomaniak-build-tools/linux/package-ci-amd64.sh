#!/usr/bin/env bash

#
# Infomaniak kDrive - Desktop
# Copyright (C) 2023-2024 Infomaniak Network SA
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

export APP_DIR=$PWD/build-linux/app
export BASE_DIR=$PWD

cd $APP_DIR

export QT_BASE_DIR=$HOME/Qt/6.2.3/gcc_64
export QTDIR=$QT_BASE_DIR
export QMAKE=$QT_BASE_DIR/bin/qmake
export PATH=$QT_BASE_DIR/bin:$QT_BASE_DIR/libexec:$PATH
export LD_LIBRARY_PATH=$QT_BASE_DIR/lib:$APP_DIR/usr/lib:/usr/local/lib:/usr/local/lib64:$LD_LIBRARY_PATH
export PKG_CONFIG_PATH=$QT_BASE_DIR/lib/pkgconfig:$PKG_CONFIG_PATH

mkdir -p $APP_DIR/usr/plugins

cp -P -r $QTDIR/plugins/* $APP_DIR/usr/plugins/
cp -P -r $QTDIR/libexec $APP_DIR/usr
cp -P -r $QTDIR/resources $APP_DIR/usr
cp -P -r $QTDIR/translations/ $APP_DIR/usr

mv $APP_DIR/usr/lib/x86_64-linux-gnu/* $APP_DIR/usr/lib/

mkdir -p $APP_DIR/usr/qml

rm -rf $APP_DIR/usr/lib/x86_64-linux-gnu/
rm -rf $APP_DIR/usr/include/

cp $BASE_DIR/sync-exclude-linux.lst $APP_DIR/usr/bin/sync-exclude.lst
cp $APP_DIR/usr/share/icons/hicolor/512x512/apps/kdrive-win.png $APP_DIR

cp -P /usr/local/lib64/libssl.so* $APP_DIR/usr/lib/
cp -P /usr/local/lib64/libcrypto.so* $APP_DIR/usr/lib/
cp -P -r /usr/lib/x86_64-linux-gnu/nss/ $APP_DIR/usr/lib/
cp ~/Qt/Tools/QtCreator/lib/Qt/lib/libQt6SerialPort.so.6 $APP_DIR/usr/lib/

$HOME/desktop-setup/linuxdeploy-x86_64.AppImage --appdir $APP_DIR -e $APP_DIR/usr/bin/kDrive -i $APP_DIR/kdrive-win.png -d $APP_DIR/usr/share/applications/kDrive_client.desktop --plugin qt --output appimage -v0

VERSION=$(grep "KDRIVE_VERSION_FULL" "$BASE_DIR/build-linux/build/version.h" | awk '{print $3}')
APP_NAME=kDrive-${VERSION}-amd64.AppImage

mv kDrive*.AppImage ../$APP_NAME
cd $BASE_DIR

if [ -z ${KDRIVE_TOKEN+x} ]; then
	echo "No kDrive token found, AppImage will not be uploaded."
else
	FILES=($APP_NAME "kDrive-amd64.dbg" "kDrive_client-amd64.dbg")
	source "$BASE_DIR/infomaniak-build-tools/upload_version.sh"

	for FILE in ${FILES[@]}; do
		upload_file $FILE "linux-amd"
	done
fi
