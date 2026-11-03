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
import QtQuick.Controls.Basic
import QtQuick.Shapes
import kDrive.UI

// Icon-only copy button of the errors page. After a click, a check mark replaces the icon for a moment and the tooltip
// confirms the copy; the owner does the copy itself on `copyRequested`.
ToolButton {
    id: root

    // Confirmation shown in the tooltip and to screen readers after a copy.
    required property string copiedText
    property bool copied: false

    signal copyRequested

    implicitWidth: IKActivities.errorsCopyIconSize + 2 * IKSpacing.s4
    implicitHeight: implicitWidth
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    display: AbstractButton.IconOnly
    Accessible.name: text
    Accessible.description: root.copied ? root.copiedText : ""
    onClicked: {
        root.copyRequested();
        root.copied = true;
        copiedTimer.restart();
    }

    Timer {
        id: copiedTimer

        interval: IKActivities.errorsCopiedFeedbackDuration
        onTriggered: root.copied = false
    }

    contentItem: Item {
        IKTintedIcon {
            anchors.centerIn: parent
            width: IKActivities.errorsCopyIconSize
            height: width
            visible: !root.copied
            source: "qrc:/assets/main/copy.svg"
            color: IKColors.textSecondary
        }

        // Same mark as IKCheckBox.
        Shape {
            id: copiedMark

            readonly property real unit: width / IKCheckBoxTokens.markReferenceSize

            anchors.centerIn: parent
            width: IKActivities.errorsCopyIconSize
            height: width
            visible: root.copied
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                strokeColor: IKColors.errorsCopiedMark
                strokeWidth: IKCheckBoxTokens.markStrokeWidth
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                startX: copiedMark.unit * 3.8
                startY: copiedMark.unit * 8.4

                PathLine {
                    x: copiedMark.unit * 6.6
                    y: copiedMark.unit * 11.2
                }

                PathLine {
                    x: copiedMark.unit * 12.2
                    y: copiedMark.unit * 5
                }
            }
        }
    }

    background: Rectangle {
        radius: IKRadius.r4
        color: root.hovered ? IKColors.surfaceTertiary : "transparent"
        border.width: root.visualFocus ? 2 : 0
        border.color: IKColors.accentPrimary
    }

    IKToolTip {
        targetButton: root
        text: root.copied ? root.copiedText : root.text
    }
}
