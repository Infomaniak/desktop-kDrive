#!/usr/bin/env bash
#------------------------------------------------------------------------------  
# Infomaniak kDrive - Desktop  
# Get version from version.json without external dependencies  
#------------------------------------------------------------------------------  

VERSION_JSON_RELATIVE_PATH="version.json"

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

    # Extract version fields
    local major minor patch build
    major=$(sed -n 's/.*"major"[[:space:]]*:[[:space:]]*\([0-9]*\).*/\1/p' "$version_json_path")
    minor=$(sed -n 's/.*"minor"[[:space:]]*:[[:space:]]*\([0-9]*\).*/\1/p' "$version_json_path")
    patch=$(sed -n 's/.*"patch"[[:space:]]*:[[:space:]]*\([0-9]*\).*/\1/p' "$version_json_path")
    build=$(sed -n 's/.*"build"[[:space:]]*:[[:space:]]*\([0-9]*\).*/\1/p' "$version_json_path")

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