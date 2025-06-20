#!/bin/bash

# Script to create the Mac installer using productbuild tool
#

[ "$#" -lt 2 ] && echo "Usage: create_mac_appstore.sh <CMAKE_INSTALL_DIR> <installer sign identity>" && exit

# the path of installation must be given as parameter
if [ -z "$1" ]; then
  echo "ERROR: Provide the path to CMAKE_INSTALL_DIR to this script as first parameter."
  exit 1
fi

install_path="$1"
identity="$2"

# The path of the app
app="@APPLICATION_SHORTNAME@"
app_path="$install_path/$app.app"

# The name of the installer package
installer="@APPLICATION_SHORTNAME@-@KDRIVE_VERSION_FULL@"
installer_file="$installer.pkg"

# Sign the finished package if desired.
if [ ! -z "$identity" ]; then
  echo "Will try to sign the installer"
  pushd $install_path
  productbuild --sign "$identity" --component "$app_path" /Applications "$installer_file.new"
  mv "$installer_file".new "$installer_file"
  popd
else
  echo "No certificate given, will not sign the pkg"
fi

