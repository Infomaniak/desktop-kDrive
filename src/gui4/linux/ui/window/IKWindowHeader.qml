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
import kDrive.UI

Item {
    id: root

    property alias backgroundData: customBackground.data
    property alias contentData: customContent.data
    required property var targetWindow
    property color backgroundColor: IKColors.surfacePrimary
    property bool titleVisible: true

    readonly property bool maximized: targetWindow.visibility === Window.Maximized

    height: IKWindow.headerHeight

    Rectangle {
        anchors.fill: parent
        color: root.backgroundColor
    }

    Item {
        id: customBackground

        anchors.fill: parent
    }

    MouseArea {
        anchors.left: parent.left
        anchors.right: windowControls.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        acceptedButtons: Qt.LeftButton

        onPressed: root.targetWindow.startSystemMove()
        onDoubleClicked: {
            if (root.maximized) {
                root.targetWindow.showNormal()
            } else {
                root.targetWindow.showMaximized()
            }
        }
    }

    Text {
        id: windowTitle

        anchors.left: parent.left
        anchors.leftMargin: IKSpacing.s16
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(implicitWidth, IKWindow.titleMaxWidth)
        visible: root.titleVisible
        text: root.targetWindow.title
        color: IKColors.textPrimary
        font.pixelSize: IKFonts.headlineSize
        font.weight: IKFonts.emphasized
        elide: Text.ElideRight
    }

    Item {
        id: customContent

        anchors.left: root.titleVisible ? windowTitle.right : parent.left
        anchors.leftMargin: IKSpacing.s16
        anchors.right: windowControls.left
        anchors.rightMargin: IKSpacing.s8
        anchors.top: parent.top
        anchors.bottom: parent.bottom
    }

    Row {
        id: windowControls

        anchors.right: parent.right
        anchors.top: parent.top
        height: parent.height
        spacing: 0

        IKWindowControlButton {
            targetWindow: root.targetWindow
            controlType: IKWindowControlButton.Minimize
        }

        IKWindowControlButton {
            targetWindow: root.targetWindow
            controlType: root.maximized ? IKWindowControlButton.Restore : IKWindowControlButton.Maximize
        }

        IKWindowControlButton {
            targetWindow: root.targetWindow
            controlType: IKWindowControlButton.Close
        }
    }
}
