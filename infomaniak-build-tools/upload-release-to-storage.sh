#!/bin/bash

# Infomaniak kDrive - Desktop App
# Copyright (C) 2023-2024 Infomaniak Network SA
#
# Licensed under the GNU General Public License v3.0

set -e

version="$1"
user="$2"
pass="$3"

if [[ -z "$version" || -z "$user" || -z "$pass" ]]; then
    echo "Usage: $0 <version> <user> <pass>"
    exit 1
fi

app="kDrive-$version"

os_list=("linux" "macos" "win")
languages=("de" "en" "es" "fr" "it")

function cleanup {
    ./mc.exe alias remove kdrive-storage || true
    echo "🧹 Cleanup completed."
}
trap cleanup EXIT

echo "🔧 Configuring storage alias..."

if ! ./mc.exe alias set kdrive-storage https://storage5.infomaniak.com "$user" "$pass" --api s3v4; then
    echo "❌ config host add kdrive-storage failed"
    exit 1
fi

# Upload release notes
for os in "${os_list[@]}"; do
    for lang in "${languages[@]}"; do
        fileName="$app-$os-$lang.html"
        filePath="./release_notes/$app/$fileName"
        if [[ ! -f "$filePath" ]]; then
            echo "❌ File $filePath does not exist, aborting upload."
            exit 1
        fi

        echo "📤 Uploading $fileName..."
        if ! ./mc.exe cp --attr Content-Type=text/html "$filePath" "kdrive-storage/download/drive/desktopclient/$fileName"; then
            echo "❌ Upload of $fileName failed"
            exit 1
        fi
    done
done

# Upload executables
executables=("$app.exe")

for executable in "${executables[@]}"; do
    filePath="./installers/$executable"
    if [[ ! -f "$filePath" ]]; then
        echo "❌ File $filePath does not exist, aborting upload."
        exit 1
    fi

    echo "📤 Uploading $executable..."
    if ! ./mc.exe cp --attr Content-Type=application/octet-stream "$filePath" "kdrive-storage/download/drive/desktopclient/$executable" --debug; then
        echo "❌ Upload of $executable failed"
        exit 1
    fi
done

echo "✅ All files uploaded successfully to kDrive storage."
