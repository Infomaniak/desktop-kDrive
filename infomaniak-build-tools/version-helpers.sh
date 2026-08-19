#!/usr/bin/env bash
#------------------------------------------------------------------------------  
# Infomaniak kDrive - Desktop  
# Get version from version.json without external dependencies  
#------------------------------------------------------------------------------  

VERSION_JSON_RELATIVE_PATH="version.json"

GetVersionFromJson() {
    local repository_root_path="$1"
    local include_build_version="${2:-true}"
    local platform="${3:-}"

    if [[ -z "$repository_root_path" ]]; then
        echo "Error: Repository root path is required." >&2
        return 1
    fi

    local version_json_path="${repository_root_path%/}/${VERSION_JSON_RELATIVE_PATH}"

    if [[ ! -f "$version_json_path" ]]; then
        echo "Error: version.json not found at path: $version_json_path" >&2
        return 1
    fi

    # Determine platform if not provided
    if [[ -z "$platform" ]]; then
        if [[ "$OSTYPE" == "linux-gnu"* ]]; then
            platform="Linux"
        elif [[ "$OSTYPE" == "darwin"* ]]; then
            platform="macOS"
        else
            echo "Error: Unsupported platform: $OSTYPE" >&2
            return 1
        fi
    else
        # Validate provided platform
        if [[ "$platform" != "Linux" && "$platform" != "macOS" && "$platform" != "Windows" ]]; then
            echo "Error: Invalid platform: $platform. Supported platforms are: Linux, macOS, Windows" >&2
            return 1
        fi
    fi

    # Extract version fields for the specified platform.
    # The platform block is scoped from its opening brace to the matching closing brace
    # so that fields are read from the correct OS object only.
    local major minor patch build
    major=$(sed -n "/\"${platform}\"[[:space:]]*:[[:space:]]*{/,/}/{s/.*\"major\"[[:space:]]*:[[:space:]]*\([0-9]\{1,\}\).*/\1/p;}" "$version_json_path" | head -n 1)
    minor=$(sed -n "/\"${platform}\"[[:space:]]*:[[:space:]]*{/,/}/{s/.*\"minor\"[[:space:]]*:[[:space:]]*\([0-9]\{1,\}\).*/\1/p;}" "$version_json_path" | head -n 1)
    patch=$(sed -n "/\"${platform}\"[[:space:]]*:[[:space:]]*{/,/}/{s/.*\"patch\"[[:space:]]*:[[:space:]]*\([0-9]\{1,\}\).*/\1/p;}" "$version_json_path" | head -n 1)
    build=$(sed -n "/\"${platform}\"[[:space:]]*:[[:space:]]*{/,/}/{s/.*\"build\"[[:space:]]*:[[:space:]]*\([0-9]\{1,\}\).*/\1/p;}" "$version_json_path" | head -n 1)

    if [[ -z "$major" || -z "$minor" || -z "$patch" ]]; then
        echo "Error: Missing version fields for platform $platform in $version_json_path" >&2
        return 1
    fi

    local version_string="${major}.${minor}.${patch}"
    if [[ "$include_build_version" == "true" && -n "$build" ]]; then
        version_string+=".${build}"
    fi

    echo "$version_string"
}