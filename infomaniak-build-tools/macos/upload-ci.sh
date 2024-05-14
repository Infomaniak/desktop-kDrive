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

VERSION=$(grep "KDRIVE_VERSION_FULL" "build-macos/client/version.h" | awk '{print $3}')
APP_NAME=kDrive-${VERSION}.pkg
mv build-macos/install/$APP_NAME build-macos
cd build-macos

tar -cvf kDrive-debug-macos.tar kDrive.dSYM kDrive_client.dSYM

if [ -z ${KDRIVE_TOKEN+x} ]; then
	echo "No kDrive token found, Package will not be uploaded."
else

	FILES=($APP_NAME "kDrive-debug-macos.tar")

	for FILE in ${FILES[@]}; do
		SIZE=$(ls -l $FILE | awk '{print $5}')

		curl -X POST \
			-H "Authorization: Bearer $KDRIVE_TOKEN" \
			-H "Content-Type: application/octet-stream" \
			--data-binary @$FILE \
			"https://api.infomaniak.com/3/drive/$KDRIVE_ID/upload?directory_id=$KDRIVE_DIR_ID&total_size=$SIZE&file_name=$FILE&directory_path=${VERSION:0:3}/${VERSION:0:5}/${VERSION:6}/macos&conflict=version"
	done
fi
