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
                text: ThemeMode._mode === ThemeMode.System ? "System"
                    : ThemeMode._mode === ThemeMode.Light  ? "Light"
                    : "Dark"
                color: IKColors.contentPrimary
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Toggle theme"
                onClicked: {
                    if (ThemeMode._mode === ThemeMode.System)
                        ThemeMode.set(ThemeMode.Dark)
                    else if (ThemeMode._mode === ThemeMode.Dark)
                        ThemeMode.set(ThemeMode.Light)
                    else
                        ThemeMode.set(ThemeMode.System)
                }
            }
        }
    }
}