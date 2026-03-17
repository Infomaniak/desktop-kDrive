import QtQuick
import QtQuick.Controls
import kDrive.UI

Window {
    id: mainWindow
    visible: true
    title: "kDrive"

    // This rectangle is only there to test the dark mode management
    // and will be removed once the gui implem will start.
    Rectangle {
        anchors.fill: parent
        color: IKColors.accentPrimary

        Column {
            anchors {
                bottom: parent.bottom
                horizontalCenter: parent.horizontalCenter
                bottomMargin: 16
            }
            spacing: 8

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: IKColors._themeMode === ThemeMode.System ? "System"
                    : IKColors._themeMode === ThemeMode.Light  ? "Light"
                    : "Dark"
                color: IKColors.textPrimary
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Toggle theme"
                onClicked: {
                    if (IKColors._themeMode === ThemeMode.System)
                        IKColors.setThemeMode(ThemeMode.Dark)
                    else if (IKColors._themeMode === ThemeMode.Dark)
                        IKColors.setThemeMode(ThemeMode.Light)
                    else
                        IKColors.setThemeMode(ThemeMode.System)
                }
            }
        }
    }
}