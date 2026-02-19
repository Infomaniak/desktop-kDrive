#!/bin/sh

# Post-install script for kDrive macOS installer
# Works with both New Swift GUI and Legacy Qt GUI
# Both are installed in nested structure: /Applications/<container>/<app>.app

# Determine app location based on installation
# New Swift GUI: /Applications/@PACKAGE_APP_CONTAINER@/@PACKAGE_APP_NAME@.app
if [ -d "/Applications/@PACKAGE_APP_CONTAINER@/@PACKAGE_APP_NAME@.app" ]; then
    KDRIVE_APP_PATH="/Applications/@PACKAGE_APP_CONTAINER@/@PACKAGE_APP_NAME@.app"
    echo "Found new Swift GUI at: $KDRIVE_APP_PATH"
# Legacy Qt GUI: /Applications/@APPLICATION_NAME_XML_ESCAPED@/@APPLICATION_EXECUTABLE@.app  
elif [ -d "/Applications/@APPLICATION_NAME_XML_ESCAPED@/@APPLICATION_EXECUTABLE@.app" ]; then
    KDRIVE_APP_PATH="/Applications/@APPLICATION_NAME_XML_ESCAPED@/@APPLICATION_EXECUTABLE@.app"
    echo "Found legacy Qt GUI at: $KDRIVE_APP_PATH"
# Fallback for old flat installations
elif [ -d "/Applications/@APPLICATION_EXECUTABLE@.app" ]; then
    KDRIVE_APP_PATH="/Applications/@APPLICATION_EXECUTABLE@.app"
    echo "Found flat installation at: $KDRIVE_APP_PATH"
else
    echo "kDrive app not found in expected locations"
    echo "Checked: /Applications/@PACKAGE_APP_CONTAINER@/@PACKAGE_APP_NAME@.app"
    echo "Checked: /Applications/@APPLICATION_NAME_XML_ESCAPED@/@APPLICATION_EXECUTABLE@.app"
    exit 1
fi

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
