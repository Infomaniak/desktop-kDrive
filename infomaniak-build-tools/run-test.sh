#! /bin/bash

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
#!/bin/bash

if [ -z "$1" ]; then
    echo -e "Error: No path provided. Usage: $0 <dir path> <filename>"
    exit 1
fi

if [ -z "$2" ]; then
    echo -e "Error: No filename provided. Usage: $0 <dir path> <filename>"
    exit 1
fi

dir=$1
tester=$2

echo "---------- Running: $tester ----------"
pushd $dir 1>/dev/null

if [ ! -f "$tester" ]; then
    echo -e "Error: File $tester does not exist."
    exit 1
fi

chmod +x $tester
lldb ./$tester -o run -o bt

if [ $? -ne 0 ]; then
    echo "---------- Failure: $tester ----------"
    exit 1
else
    echo "---------- Success: $tester ----------"
fi

popd 1>/dev/null
exit 0
