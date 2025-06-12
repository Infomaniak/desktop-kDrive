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

version=$(grep "KDRIVE_VERSION_FULL" "build-macos/client/version.h" | awk '{print $3}')
app_name="kDrive-$version.pkg"
mv "build-macos/client/install/$app_name" build-macos
cd build-macos

tar -cvf kDrive-debug-macos.tar kDrive.dSYM kDrive_client.dSYM

if [ -z ${KDRIVE_TOKEN+x} ]; then
	echo "No kDrive token found, Package will not be uploaded."
else

	FILES=("$app_name" "kDrive-debug-macos.tar")
	source "../infomaniak-build-tools/upload_version.sh"

	for FILE in ${FILES[@]}; do
		upload_file "$FILE" "macos"
	done
fi
