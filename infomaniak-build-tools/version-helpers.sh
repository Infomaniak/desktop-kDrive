#!/usr/bin/env bash
#------------------------------------------------------------------------------
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
#------------------------------------------------------------------------------

# Relative path to version.json from repository root
VERSION_JSON_RELATIVE_PATH="version.json"

# Usage:
#   GetVersionFromJson <repository_root_path> [include_build_version=true]
#
# Example:
#   version=$(GetVersionFromJson "/home/user/projects/desktop-kDrive" false)
#   echo "Version: $version"
#
# Requires: jq
#------------------------------------------------------------------------------

GetVersionFromJson() {
    local repository_root_path="$1"
    local include_build_version="${2:-true}"

    if [[ -z "$repository_root_path" ]]; then
        echo "Error: Repository root path is required." >&2
        return 1
    fi

    local version_json_path="${repository_root_path%/}/${VERSION_JSON_RELATIVE_PATH}"

    if [[ ! -f "$version_json_path" ]]; then
        echo "Error: version.json not found at path: $version_json_path" >&2
        return 1
    fi

    # Parse the JSON using jq
    local major minor patch build
    major=$(jq -r '.NextVersion.major' "$version_json_path")
    minor=$(jq -r '.NextVersion.minor' "$version_json_path")
    patch=$(jq -r '.NextVersion.patch' "$version_json_path")
    build=$(jq -r '.NextVersion.build // empty' "$version_json_path")

    if [[ -z "$major" || -z "$minor" || -z "$patch" ]]; then
        echo "Error: Missing version fields in $version_json_path" >&2
        return 1
    fi

    local version_string="${major}.${minor}.${patch}"
    if [[ "$include_build_version" == "true" && -n "$build" ]]; then
        version_string+=".${build}"
    fi

    echo "$version_string"
}