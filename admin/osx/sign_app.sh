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

[ "$#" -lt 2 ] && echo "Usage: sign_app.sh <app> <identity> <team_identifier> <app_domain>" && exit

src_app="$1"
identity="$2"
team_identifier="$3"
app_domain="$4"

echo "Signing src_app:$src_app identity:$identity team_identifier:$team_identifier app_domain:$app_domain"

codesign -s "$identity" --force --verbose=4 --options=runtime "$src_app/Contents/Frameworks/Sparkle.framework/Versions/B/XPCServices/Installer.xpc"
codesign -s "$identity" --force --verbose=4 --options=runtime "$src_app/Contents/Frameworks/Sparkle.framework/Versions/B/XPCServices/Downloader.xpc"
codesign -s "$identity" --force --verbose=4 --options=runtime "$src_app/Contents/Frameworks/Sparkle.framework/Versions/B/Autoupdate"
codesign -s "$identity" --force --verbose=4 --options=runtime "$src_app/Contents/Frameworks/Sparkle.framework/Versions/B/Updater.app"
codesign -s "$identity" --force --verbose=4 --options=runtime "$src_app/Contents/Frameworks/Sparkle.framework"

codesign -s "$identity" --force --verbose=4 --deep --options=runtime --entitlements $(dirname $0)/FinderSyncLoginItemAgent.entitlements "$src_app/Contents/Library/LoginItems/$team_identifier.$app_domain.LoginItemAgent.app"
codesign -s "$identity" --force --verbose=4 --deep --options=runtime --entitlements $(dirname $0)/FinderSyncExtension.entitlements "$src_app/Contents/PlugIns/Extension.appex"
codesign -s "$identity" --force --verbose=4 --deep --options=runtime --entitlements $(dirname $0)/LiteSyncExtension.entitlements "$src_app/Contents/Library/SystemExtensions/$app_domain.LiteSyncExt.systemextension"

# kdrive4 GUI
codesign -s "$identity" --force --verbose=4 --options=runtime "$src_app/Contents/MacOS/kDrive_client4.app/Contents/MacOS/kDrive.gui"

# kdrive4 lib
# /Users/adrien/dev2/desktop-kDrive/build-macos/client/install/kDrive.app/Contents/MacOS/kDrive_client4.app/Contents/Frameworks/kDriveCore.framework/kDriveCore
 
#codesign -s "$identity" --force --verbose=4 --options=runtime "$src_app/Contents/MacOS/kDrive_client4.app/Contents/Frameworks/kDriveCore.framework/kDriveCore"
codesign -s "$identity" --force --verbose=4 --options=runtime "$src_app/Contents/MacOS/kDrive_client4.app/Contents/Frameworks/kDriveCore.framework/Versions/A/kDriveCore"
codesign -s "$identity" --force --verbose=4 --options=runtime "$src_app/Contents/MacOS/kDrive_client4.app/Contents/Frameworks/kDriveCore.framework"

#codesign -s "$identity" --force --verbose=4 --options=runtime "$src_app/Contents/MacOS/kDrive_client4.app/Contents/Frameworks/kDriveResources.framework/kDriveResources"
codesign -s "$identity" --force --verbose=4 --options=runtime "$src_app/Contents/MacOS/kDrive_client4.app/Contents/Frameworks/kDriveResources.framework/Versions/A/kDriveResources"
codesign -s "$identity" --force --verbose=4 --options=runtime "$src_app/Contents/MacOS/kDrive_client4.app/Contents/Frameworks/kDriveResources.framework"

#codesign -s "$identity" --force --verbose=4 --options=runtime "$src_app/Contents/MacOS/kDrive_client4.app/Contents/Frameworks/kDriveCoreUI.framework/Versions/A/kDriveCoreUI"
#codesign -s "$identity" --force --verbose=4 --options=runtime "$src_app/Contents/MacOS/kDrive_client4.app/Contents/Frameworks/kDriveCoreUI.framework/Versions/A/Frameworks/kDriveCore.framework/kDriveCore"
codesign -s "$identity" --force --verbose=4 --options=runtime "$src_app/Contents/MacOS/kDrive_client4.app/Contents/Frameworks/kDriveCoreUI.framework/Versions/Current/kDriveCoreUI"
#codesign -s "$identity" --force --verbose=4 --options=runtime "$src_app/Contents/MacOS/kDrive_client4.app/Contents/Frameworks/kDriveCoreUI.framework/Versions/Current/Frameworks/kDriveCore.framework/kDriveCore"
#codesign -s "$identity" --force --verbose=4 --options=runtime "$src_app/Contents/MacOS/kDrive_client4.app/Contents/Frameworks/kDriveCoreUI.framework/Frameworks/kDriveCore.framework/kDriveCore"

codesign -s "$identity" --force --verbose=4 --options=runtime "$src_app/Contents/MacOS/kDrive_client4.app/Contents/Frameworks/kDriveCoreUI.framework"

# kDrive_client4.app
codesign -s "$identity" --force --verbose=4 --deep --options=runtime --preserve-metadata=entitlements "$src_app/Contents/MacOS/kDrive_client4.app"
codesign -s "$identity" --force --verbose=4 --options=runtime --entitlements $(dirname $0)/kDrive4.entitlements "$src_app/Contents/MacOS/kDrive_client4.app"

codesign -s "$identity" --force --verbose=4 --deep --options=runtime --preserve-metadata=entitlements "$src_app"
codesign -s "$identity" --force --verbose=4 --options=runtime --entitlements $(dirname $0)/kDrive.entitlements "$src_app"
codesign -s "$identity" --force --verbose=4 --options=runtime --entitlements $(dirname $0)/kDriveUninstaller.entitlements "$src_app/Contents/Frameworks/kDrive Uninstaller.app"

# Verify the signature
codesign -dv $src_app
codesign --verify -v --strict $src_app
# spctl -a -t exec -vv $src_app

# Validate that the key used for signing the binary matches the expected TeamIdentifier
codesign -dv $src_app 2>&1 | grep "TeamIdentifier=$team_identifier"
exit $?