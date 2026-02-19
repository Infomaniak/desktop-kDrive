#!/bin/sh

# Post-install script for kDrive macOS installer
# Works with both New Swift GUI and Legacy Qt GUI

# Determine app location based on installation
if [ -d "/Applications/@PACKAGE_APP_NAME@.app" ]; then
    # New Swift GUI or standard installation
    KDRIVE_APP_PATH="/Applications/@PACKAGE_APP_NAME@.app"
elif [ -d "/Applications/@APPLICATION_EXECUTABLE@/@APPLICATION_EXECUTABLE@.app" ]; then
    # Legacy Qt GUI nested installation
    KDRIVE_APP_PATH="/Applications/@APPLICATION_EXECUTABLE@/@APPLICATION_EXECUTABLE@.app"
elif [ -d "/Applications/@APPLICATION_EXECUTABLE@.app" ]; then
    # Legacy Qt GUI flat installation
    KDRIVE_APP_PATH="/Applications/@APPLICATION_EXECUTABLE@.app"
else
    echo "kDrive app not found in expected locations"
    exit 1
fi

echo "Found kDrive at: $KDRIVE_APP_PATH"

# Always enable the finder plugin if available
if [ -x "$(command -v pluginkit)" ]; then
    # add it to DB. This happens automatically too but we try to push it a bit harder for issue #3463
    pluginkit -a "$KDRIVE_APP_PATH/Contents/PlugIns/Extension.appex/"
    # Since El Capitan we need to sleep #4650
    sleep 10s
    # enable it
    pluginkit -e use -i @APPLICATION_REV_DOMAIN@.Extension
fi

# Launch the application
su "$USER" -c "open \"$KDRIVE_APP_PATH\""
exit 0
