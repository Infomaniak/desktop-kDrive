/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import QtQuick
import QtQuick.Controls
import kDrive.UI

Window {
    id: mainWindow
    visible: false
    title: "kDrive"

    onClosing: (close) => {
        close.accepted = false;
        systemTrayController.hideMainWindow();
    }

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
                color: IKColors.textPrimary
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Toggle theme"
                onClicked: {
                    if (ThemeMode._mode === ThemeMode.System)
                        ThemeMode.set(ThemeMode.Dark);
                    else if (ThemeMode._mode === ThemeMode.Dark)
                        ThemeMode.set(ThemeMode.Light);
                    else
                        ThemeMode.set(ThemeMode.System);
                }
            }
        }
    }
}
