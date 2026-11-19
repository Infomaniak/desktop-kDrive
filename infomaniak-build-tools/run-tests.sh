#! /bin/bash

#
# Infomaniak kDrive - Desktop
# Copyright (C) 2023-2026 Infomaniak Network SA
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
# Color coding
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

# An explicit build root prevents executing stale binaries from another build.
build_root=$(cd "${1:-.}" && pwd) || exit 1
conanrun=$(find "$build_root" -type f -name "conanrun.sh" -print -quit)
if [[ -n "$conanrun" ]]; then
    # shellcheck disable=SC1090
    source "$conanrun" || exit 1
else
    echo -e "${RED}conanrun.sh not found${NC}"
    exit 1
fi

testers=()
while IFS= read -r -d '' tester; do
    testers+=("$tester")
done < <(find "$build_root" -type f -name "kDrive_test_*" -print0)
if [[ ${#testers[@]} -eq 0 ]]; then
    echo -e "${RED}No test executables found in $build_root${NC}" >&2
    exit 1
fi
errors=0
failures=()

for tester in "${testers[@]}"; do
    echo -e "${YELLOW}---------- Running $(basename "$tester") ----------${NC}"
    if ! (
        chmod +x "$tester" || exit 1
        cd "$(dirname "$tester")" || exit 1
        "$tester"
    ); then
        (( errors+=1 ))
        failures+=("$(basename "$tester")")
        echo -e "${RED}---------- Failure: $(basename "$tester") ----------${NC}"
    else
        echo -e "${GREEN}---------- Success: $(basename "$tester") ----------${NC}"
    fi
done

if [ $errors -eq 0 ]; then
    echo -e "${GREEN}Success: All Tests passed !${NC}"
else
    echo -e "${RED}Failures :\n"
    for failure in "${failures[@]}"; do
        echo -e "$failure"
    done
    echo -e "${NC}"
fi
exit $errors
