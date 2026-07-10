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

ToolButton {
    id: root

    enum ControlType {
        Minimize,
        Maximize,
        Restore,
        Close
    }

    required property var targetWindow
    required property int controlType

    readonly property bool closeControl: controlType === IKWindowControlButton.Close
    readonly property color iconColor: closeControl && (hovered || down)
                                                ? IKColors.windowControlIconOnClose
                                                : IKColors.windowControlIcon

    width: IKWindow.controlButtonWidth
    height: IKWindow.headerHeight
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    display: AbstractButton.IconOnly
    text: {
        switch (controlType) {
        case IKWindowControlButton.Minimize:
            return qsTrId("buttonMinimize");
        case IKWindowControlButton.Maximize:
            return qsTrId("buttonMaximize");
        case IKWindowControlButton.Restore:
            return qsTrId("buttonRestore");
        case IKWindowControlButton.Close:
            return qsTrId("buttonClose");
        }
        return ""
    }

    onClicked: {
        switch (controlType) {
        case IKWindowControlButton.Minimize:
            targetWindow.showMinimized()
            break
        case IKWindowControlButton.Maximize:
            targetWindow.showMaximized()
            break
        case IKWindowControlButton.Restore:
            targetWindow.showNormal()
            break
        case IKWindowControlButton.Close:
            targetWindow.close()
            break
        }
    }

    ToolTip.visible: hovered
    ToolTip.text: text
    ToolTip.delay: 500

    background: Rectangle {
        color: {
            if (root.closeControl) {
                if (root.down) {
                    return IKColors.windowControlClosePressed
                }
                if (root.hovered) {
                    return IKColors.windowControlCloseHover
                }
                return "transparent"
            }
            if (root.down) {
                return IKColors.windowControlPressed
            }
            if (root.hovered) {
                return IKColors.windowControlHover
            }
            return "transparent"
        }
        border.width: root.visualFocus ? 2 : 0
        border.color: IKColors.accentPrimary
    }

    contentItem: Item {
        Rectangle {
            anchors.centerIn: parent
            width: IKWindow.controlIconSize
            height: 1
            color: root.iconColor
            visible: root.controlType === IKWindowControlButton.Minimize
        }

        Rectangle {
            anchors.centerIn: parent
            width: IKWindow.controlIconSize
            height: IKWindow.controlIconSize
            color: "transparent"
            border.width: 1
            border.color: root.iconColor
            visible: root.controlType === IKWindowControlButton.Maximize
        }

        Item {
            anchors.centerIn: parent
            width: IKWindow.controlIconSize
            height: IKWindow.controlIconSize
            visible: root.controlType === IKWindowControlButton.Restore

            Rectangle {
                x: 2
                y: 0
                width: parent.width - 2
                height: parent.height - 2
                color: "transparent"
                border.width: 1
                border.color: root.iconColor
            }

            Rectangle {
                x: 0
                y: 2
                width: parent.width - 2
                height: parent.height - 2
                color: root.down
                       ? IKColors.windowControlPressed
                       : (root.hovered ? IKColors.windowControlHover : root.targetWindow.surfaceColor)
                border.width: 1
                border.color: root.iconColor
            }
        }

        Item {
            anchors.centerIn: parent
            width: IKWindow.controlIconSize
            height: IKWindow.controlIconSize
            visible: root.controlType === IKWindowControlButton.Close

            Rectangle {
                anchors.centerIn: parent
                width: parent.width + 2
                height: 1
                color: root.iconColor
                rotation: 45
            }

            Rectangle {
                anchors.centerIn: parent
                width: parent.width + 2
                height: 1
                color: root.iconColor
                rotation: -45
            }
        }
    }
}
