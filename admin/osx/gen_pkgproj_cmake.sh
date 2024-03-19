#!/bin/bash

#
# Infomaniak kDrive - Desktop
# Copyright (C) 2023-2024 Infomaniak Network SA
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

IN=$1
if [ -z "$IN" ]; then
  echo "Usage : pkgproj_to_cmake.sh <INPUT FILE>"
  exit 22
else
  if ! [ -f "$IN" ]; then
    echo "File does not exist."
    exit 2
  fi
fi

APP_EXEC="kDrive.app"
APP_EXEC_CMAKE="@APPLICATION_EXECUTABLE@.app"

POST_INSTALL="/Users/.*/Projects/.*macOS-Debug.*/admin/osx/post_install.sh"
POST_INSTALL_CMAKE="@CMAKE_CURRENT_BINARY_DIR@/post_install.sh"

PRE_INSTALL="/Users/.*/Projects/.*macOS-Debug.*/admin/osx/pre_install.sh"
PRE_INSTALL_CMAKE="@CMAKE_CURRENT_BINARY_DIR@/pre_install.sh"

REV_DOMAIN_INSTALL="com.infomaniak.drive.desktopclient"
REV_DOMAIN_INSTALL_CMAKE="@APPLICATION_REV_DOMAIN@"

APP_NAME_ESCAPED="kDrive"
APP_NAME_ESCAPED_CMAKE="@APPLICATION_NAME_XML_ESCAPED@"

KDRIVE_VERSION="[0-9]*\.[0-9]*\.[0-9]*\.[0-9]*"
KDRIVE_VERSION_CMAKE="@KDRIVE_VERSION_FULL@"

APP_EXEC_NAME="kDrive Client"
APP_EXEC_NAME_CMAKE="@APPLICATION_NAME_XML_ESCAPED@ Client"

BIN_PATH="/Users/.*/Projects/.*macOS-Debug.*/bin"
BIN_PATH_CMAKE=""

APP_INSTALLER_NAME="kDrive Installer"
APP_INSTALLER_NAME_CMAKE="@APPLICATION_NAME_XML_ESCAPED@ Installer"

INTRODUCTION_FILE="/Users/.*/Projects/kdrive.*/admin/osx/installer-assets/Introduction.rtfd"
INTRODUCTION_FILE_CMAKE="@MAC_INSTALLER_INTRODUCTION_FILE@"

BACKGROUND_FILE="/Users/.*/Projects/kdrive.*/admin/osx/installer-assets/installer-background.png"
BACKGROUND_FILE_CMAKE="@MAC_INSTALLER_BACKGROUND_FILE@"

BUILD_PATH="<string>/.</string>"
BUILD_PATH_CMAKE="<string>@CMAKE_INSTALL_PREFIX@/.</string>"

sed -e "s/$APP_EXEC/$APP_EXEC_CMAKE/
        s#$POST_INSTALL#$POST_INSTALL_CMAKE#
        s#$PRE_INSTALL#$PRE_INSTALL_CMAKE#
        s#$REV_DOMAIN_INSTALL#$REV_DOMAIN_INSTALL_CMAKE#
        s#$APP_NAME_ESCAPED#$APP_NAME_ESCAPED_CMAKE#
        s#$KDRIVE_VERSION#$KDRIVE_VERSION_CMAKE#
        s#$APP_EXEC_NAME#$APP_EXEC_NAME_CMAKE#
        s#$BIN_PATH#$BIN_PATH_CMAKE#
        s#$APP_INSTALLER_NAME#$APP_INSTALLER_NAME_CMAKE#
        s#$INTRODUCTION_FILE#$INTRODUCTION_FILE_CMAKE#
        s#$BACKGROUND_FILE#$BACKGROUND_FILE_CMAKE#
        s#$BUILD_PATH#$BUILD_PATH_CMAKE#" "$IN" >macosx.pkgproj.cmake
