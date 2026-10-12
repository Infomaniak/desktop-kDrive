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

pragma ComponentBehavior: Bound

import QtQuick
import kDrive.UI

// Replaces the whole main window content while the server requires a newer version. Without an installer on Linux, the
// user downloads the new version, quits kDrive and installs it.
Item {
    id: root

    signal downloadRequested
    signal quitRequested

    IKBlockingState {
        anchors.fill: parent
        anchors.leftMargin: IKBlockingStateTokens.updateRequiredMargin
        anchors.rightMargin: IKBlockingStateTokens.updateRequiredMargin
        anchors.bottomMargin: IKBlockingStateTokens.updateRequiredMargin
        anchors.topMargin: IKBlockingStateTokens.updateRequiredTopMargin
        title: qsTrId("updateRequiredTitle")
        description: qsTrId("updateRequiredDescription")

        visual: Image {
            readonly property string sourceBase: "qrc:/assets/updaterequired/folders-stack-arrows-counterclockwise"

            width: IKBlockingStateTokens.updateRequiredIllustrationWidth
            height: IKBlockingStateTokens.updateRequiredIllustrationHeight
            source: sourceBase + (ThemeMode.isDark ? "-dark" : "") + ".svg"
            fillMode: Image.PreserveAspectFit
            sourceSize.width: width
            sourceSize.height: height
            Accessible.ignored: true
        }

        actions: [
            IKModalButton {
                text: qsTrId("statusBarQuitApp")
                role: IKModalButton.Tonal
                onClicked: root.quitRequested()
            },
            IKModalButton {
                text: qsTrId("buttonDownloadManually")
                external: true
                onClicked: root.downloadRequested()
            }
        ]
    }
}
