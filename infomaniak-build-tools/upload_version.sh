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

if [ "${BASH_SOURCE[0]}" -ef "$0" ]; then
    echo "This script must be sourced, not executed."
    exit 1
fi

upload_file () {
    FILE="$1"
    SYSTEM="$2"

    SIZE=$(ls -l $FILE | awk '{print $5}')

    curl -X POST \
        -H "Authorization: Bearer $KDRIVE_TOKEN" \
        -H "Content-Type: application/octet-stream" \
        --data-binary @$FILE \
        "https://api.infomaniak.com/3/drive/$KDRIVE_ID/upload?directory_id=$KDRIVE_DIR_ID&total_size=$SIZE&file_name=$FILE&directory_path=${VERSION:0:3}/${VERSION:0:5}/${VERSION:6}/${SYSTEM}&conflict=version"
}
