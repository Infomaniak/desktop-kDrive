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

conanrun=$(find . -type f -name "conanrun.sh" | head -n 1)
if [[ -n "$conanrun" ]]; then
    source "$conanrun"
else
    echo -e "${RED}conanrun.sh not found${NC}"
    exit 1
fi

testers=$(find . -type f -name "kDrive_test_*")
errors=0
failures=()

for tester in ${testers[@]}; do
    echo -e "${YELLOW}---------- Running $(basename $tester) ----------${NC}"
    chmod +x $tester
    pushd $(dirname "$tester") 1>/dev/null
    ./$(basename "$tester")

    if [ $? -ne 0 ]; then
        (( errors+=1 ))
        failures+=($(basename $tester))
        echo -e "${RED}---------- Failure: $(basename $tester) ----------${NC}";
    else
        echo -e "${GREEN}---------- Success: $(basename $tester) ----------${NC}";
    fi
    popd 1>/dev/null
done

if [ $errors -eq 0 ]; then
    echo -e "${GREEN}Success: All Tests passed !${NC}"
else
    echo -e "${RED}Failures :\n"
    for failure in ${failures[@]}; do
        echo -e "$failure"
    done
    echo -e "${NC}"
fi
exit $errors