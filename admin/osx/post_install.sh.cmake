#!/bin/sh

app_path="/Applications/@APPLICATION_NAME@/@APPLICATION_EXECUTABLE@.app"
uninstaller_source="$app_path/Contents/Frameworks/kDrive Uninstaller.app"
uninstaller_destination="/Applications/@APPLICATION_NAME@/kDrive Uninstaller.app"

if [ -d "$uninstaller_source" ]; then
    rm -rf "$uninstaller_destination"
    cp -R "$uninstaller_source" "$uninstaller_destination"
fi

# Always enable the finder plugin if available
if [ -x "$(command -v pluginkit)" ]; then
    # add it to DB. This happens automatically too but we try to push it a bit harder for issue #3463
    pluginkit -a  "$app_path/Contents/PlugIns/Extension.appex/"
    # Since El Capitan we need to sleep #4650
    sleep 10s
    # enable it
    pluginkit -e use -i @APPLICATION_REV_DOMAIN@.Extension
fi

console_user="$(stat -f "%Su" /dev/console)"
if [ -n "$console_user" ] && [ "$console_user" != "root" ]; then
    console_uid="$(id -u "$console_user")"
    launchctl asuser "$console_uid" sudo -u "$console_user" open "$app_path"
else
    open "$app_path"
fi
exit 0
