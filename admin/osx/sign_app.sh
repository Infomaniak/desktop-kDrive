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

echo "Signing Qt Frameworks..."
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/Frameworks/QtCore.framework/Versions/A/QtCore"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/Frameworks/QtDBus.framework/Versions/A/QtDBus"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/Frameworks/QtGui.framework/Versions/A/QtGui"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/Frameworks/QtNetwork.framework/Versions/A/QtNetwork"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/Frameworks/QtSql.framework/Versions/A/QtSql"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/Frameworks/QtSvg.framework/Versions/A/QtSvg"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/Frameworks/QtSvgWidgets.framework/Versions/A/QtSvgWidgets"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/Frameworks/QtWidgets.framework/Versions/A/QtWidgets"

echo "Signing third-party libraries..."
for lib in "$src_app/Contents/Frameworks"/*.dylib; do
    codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$lib"
done

echo "Signing plugins..."
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/PlugIns/iconengines/libqsvgicon.dylib"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/PlugIns/imageformats/libqgif.dylib"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/PlugIns/imageformats/libqico.dylib"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/PlugIns/imageformats/libqjpeg.dylib"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/PlugIns/imageformats/libqsvg.dylib"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/PlugIns/platforms/libqcocoa.dylib"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/PlugIns/sqldrivers/libqsqlite.dylib"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/PlugIns/sqldrivers/libqsqlodbc.dylib"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/PlugIns/sqldrivers/libqsqlpsql.dylib"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/PlugIns/styles/libqmacstyle.dylib"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/PlugIns/tls/libqcertonlybackend.dylib"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/PlugIns/tls/libqsecuretransportbackend.dylib"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/PlugIns/kDrive_vfs_mac.dylib"

echo "Signing Extension.appex binary..."
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp --entitlements "$(dirname "$0")/FinderSyncExtension.entitlements" "$src_app/Contents/PlugIns/Extension.appex/Contents/MacOS/Extension"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp --entitlements "$(dirname "$0")/FinderSyncExtension.entitlements" "$src_app/Contents/PlugIns/Extension.appex"

echo "Signing LoginItemAgent..."
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp --entitlements "$(dirname "$0")/FinderSyncLoginItemAgent.entitlements" "$src_app/Contents/Library/LoginItems/$team_identifier.$app_domain.LoginItemAgent.app/Contents/MacOS/$team_identifier.$app_domain.LoginItemAgent"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp --entitlements "$(dirname "$0")/FinderSyncLoginItemAgent.entitlements" "$src_app/Contents/Library/LoginItems/$team_identifier.$app_domain.LoginItemAgent.app"

echo "Signing LiteSyncExt system extension..."
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp --entitlements "$(dirname "$0")/LiteSyncExtension.entitlements" "$src_app/Contents/Library/SystemExtensions/$app_domain.LiteSyncExt.systemextension/Contents/MacOS/$app_domain.LiteSyncExt"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp --entitlements "$(dirname "$0")/LiteSyncExtension.entitlements" "$src_app/Contents/Library/SystemExtensions/$app_domain.LiteSyncExt.systemextension"

echo "Signing Sparkle components..."
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/Frameworks/Sparkle.framework/Versions/B/XPCServices/Installer.xpc"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/Frameworks/Sparkle.framework/Versions/B/XPCServices/Downloader.xpc"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/Frameworks/Sparkle.framework/Versions/B/Autoupdate"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/Frameworks/Sparkle.framework/Versions/B/Updater.app"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/Frameworks/Sparkle.framework"

echo "Signing kDrive Uninstaller.app..."
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp --entitlements "$(dirname "$0")/kDriveUninstaller.entitlements" "$src_app/Contents/Frameworks/kDrive Uninstaller.app"

echo "Signing kDrive_client4.app nested frameworks (deep)..."
# Sign inner framework binary first (kDriveCore inside kDriveCoreUI) - sign Versions/A binary to avoid "bundle format ambiguous" error
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/MacOS/kDrive_client4.app/Contents/Frameworks/kDriveCoreUI.framework/Versions/A/Frameworks/kDriveCore.framework/Versions/A/kDriveCore"
# Sign frameworks at top level - sign the binaries directly for reliability
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/MacOS/kDrive_client4.app/Contents/Frameworks/kDriveCore.framework/Versions/A/kDriveCore"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/MacOS/kDrive_client4.app/Contents/Frameworks/kDriveResources.framework/Versions/A/kDriveResources"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/MacOS/kDrive_client4.app/Contents/Frameworks/InfomaniakDI_-4775B9D5F9C3466A_PackageProduct.framework/Versions/A/InfomaniakDI_-4775B9D5F9C3466A_PackageProduct"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/MacOS/kDrive_client4.app/Contents/Frameworks/kDriveCoreUI.framework/Versions/A/kDriveCoreUI"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/MacOS/kDrive_client4.app/Contents/Frameworks/Lottie.framework/Versions/A/Lottie"

echo "Sign GUI4 (kDrive_client4.app)..."
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp --entitlements "$(dirname "$0")/kDrive4.entitlements" "$src_app/Contents/MacOS/kDrive_client4.app"

echo "Signing MacOS binaries..."
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/MacOS/crashpad_handler"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/MacOS/kDrive_client"
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp "$src_app/Contents/MacOS/kDrive"

echo "Signing main app bundle..."
codesign -s "$identity" --force --verbose=4 --options=runtime --timestamp --entitlements "$(dirname "$0")/kDrive.entitlements" "$src_app"

echo "Verify the signature"
codesign -dv "$src_app"
codesign --verify -v --strict "$src_app"

echo "Validate the key used"
codesign -dv "$src_app" 2>&1 | grep "TeamIdentifier=$team_identifier"
exit $?
