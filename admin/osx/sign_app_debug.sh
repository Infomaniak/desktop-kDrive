#!/bin/sh -xe

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

[ "$#" -lt 5 ] && echo "Usage: sign_app.sh <target> <app> <identity> <team_identifier> <app_domain>" && exit 1

target="$1"

if [ $target != 'kDrive' ]; then 
   exit 0
fi

src_app="$2"
identity="$3"
team_identifier="$4"
app_domain="$5"

codesign -s "$identity" --force --verbose=4 --options=runtime "$src_app/Contents/Frameworks/Sparkle.framework/Versions/B/XPCServices/Installer.xpc"
codesign -s "$identity" --force --verbose=4 --options=runtime "$src_app/Contents/Frameworks/Sparkle.framework/Versions/B/XPCServices/Downloader.xpc"
codesign -s "$identity" --force --verbose=4 --options=runtime "$src_app/Contents/Frameworks/Sparkle.framework/Versions/B/Autoupdate"
codesign -s "$identity" --force --verbose=4 --options=runtime "$src_app/Contents/Frameworks/Sparkle.framework/Versions/B/Updater.app"
codesign -s "$identity" --force --verbose=4 --options=runtime "$src_app/Contents/Frameworks/Sparkle.framework"

codesign -s "$identity" --force --verbose=4 --deep --options=runtime --entitlements $(dirname $0)/QtWebEngineProcess.entitlements "$src_app/Contents/Frameworks/QtWebEngineCore.framework/Helpers/QtWebEngineProcess.app/Contents/MacOS/QtWebEngineProcess"
codesign -s "$identity" --force --verbose=4 --deep --options=runtime --entitlements $(dirname $0)/FinderSyncLoginItemAgent.entitlements "$src_app/Contents/Library/LoginItems/$team_identifier.$app_domain.LoginItemAgent.app"
codesign -s "$identity" --force --verbose=4 --deep --options=runtime --entitlements $(dirname $0)/FinderSyncExtension.entitlements "$src_app/Contents/PlugIns/Extension.appex"
codesign -s "$identity" --force --verbose=4 --deep --options=runtime --entitlements $(dirname $0)/kDriveUninstaller.entitlements "$src_app/Contents/Frameworks/kDrive Uninstaller.app"

codesign -s "$identity" --force --verbose=4 --deep --options=runtime --preserve-metadata=entitlements "$src_app"
codesign -s "$identity" --force --verbose=4 --options=runtime --entitlements $(dirname $0)/kDriveDebug.entitlements "$src_app"

# Verify the signature
codesign -dv $src_app
codesign --verify -v --strict $src_app
# spctl -a -t exec -vv $src_app

# Validate that the key used for signing the binary matches the expected TeamIdentifier
codesign -dv $src_app 2>&1 | grep "TeamIdentifier=$team_identifier"
exit $?