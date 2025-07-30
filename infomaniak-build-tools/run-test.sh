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

# Color coding
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

if [ -z "$1" ]; then
    echo -e "${RED}Error: No path provided. Usage: $0 <dir path> <filename>${NC}"
    exit 1
fi

if [ -z "$2" ]; then
    echo -e "${RED}Error: No filename provided. Usage: $0 <dir path> <filename>${NC}"
    exit 1
fi

dir="$1"
tester="$2"

echo "${YELLOW}---------- Running $tester ----------${NC}"
pushd "$dir" 1>/dev/null

if [ ! -f "$tester" ]; then
    echo -e "${RED}Error: File $tester does not exist.${NC}"
    exit 1
fi

chmod +x "$tester"
export DYLD_LIBRARY_PATH="$PWD:/usr/local/lib:/usr/lib:$DYLD_LIBRARY_PATH"
"./$tester"

if [ $? -ne 0 ]; then
    echo "${RED}---------- Failure: $tester ----------${NC}"
    exit 1
else
    echo "${GREEN}---------- Success: $tester ----------${NC}"
fi

popd 1>/dev/null
exit 0
