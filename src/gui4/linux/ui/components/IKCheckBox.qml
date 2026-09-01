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
import QtQuick.Controls
import QtQuick.Shapes
import kDrive.UI

// Tri-state checkbox indicator. It reports clicks and renders the state it is given; the owner decides how a click
// advances the state, so a model-driven cycle stays authoritative.
AbstractButton {
    id: root

    property int checkState: Qt.Unchecked

    readonly property bool selected: checkState !== Qt.Unchecked
    readonly property color indicatorColor: {
        if (!enabled) {
            return IKColors.checkboxDisabledSurface
        }
        return selected ? IKColors.checkboxSelectedSurface : IKColors.checkboxSurface
    }
    readonly property color indicatorBorderColor: {
        if (!enabled) {
            return IKColors.checkboxDisabledBorder
        }
        if (selected) {
            return IKColors.checkboxSelectedSurface
        }
        return hovered ? IKColors.checkboxHoverBorder : IKColors.checkboxBorder
    }

    implicitWidth: IKCheckBoxTokens.hitAreaSize
    implicitHeight: IKCheckBoxTokens.hitAreaSize
    padding: 0
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    opacity: enabled ? 1 : IKCheckBoxTokens.disabledOpacity
    Accessible.role: Accessible.CheckBox
    Accessible.checkable: enabled
    Accessible.checked: checkState === Qt.Checked
    Accessible.checkStateMixed: checkState === Qt.PartiallyChecked
    // The state belongs to the owner, so an assistive activation reports a click like the pointer and the keyboard do.
    Accessible.onPressAction: root.clicked()
    Accessible.onToggleAction: root.clicked()

    // The control sizes background and contentItem itself, so neither is anchored here.
    background: Rectangle {
        radius: IKCheckBoxTokens.focusRingRadius
        color: "transparent"
        border.width: root.visualFocus ? IKCheckBoxTokens.focusRingWidth : 0
        border.color: IKColors.accentPrimary
    }

    contentItem: Item {
        Rectangle {
            id: indicator

            anchors.centerIn: parent
            width: IKCheckBoxTokens.indicatorSize
            height: IKCheckBoxTokens.indicatorSize
            radius: IKCheckBoxTokens.indicatorRadius
            color: root.indicatorColor
            border.width: IKCheckBoxTokens.borderWidth
            border.color: root.indicatorBorderColor

            Shape {
                id: checkMark

                readonly property real unit: width / IKCheckBoxTokens.markReferenceSize

                anchors.fill: parent
                visible: root.checkState === Qt.Checked
                preferredRendererType: Shape.CurveRenderer

                ShapePath {
                    strokeColor: IKColors.checkboxMark
                    strokeWidth: IKCheckBoxTokens.markStrokeWidth
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    startX: checkMark.unit * 3.8
                    startY: checkMark.unit * 8.4

                    PathLine {
                        x: checkMark.unit * 6.6
                        y: checkMark.unit * 11.2
                    }

                    PathLine {
                        x: checkMark.unit * 12.2
                        y: checkMark.unit * 5
                    }
                }
            }

            Rectangle {
                anchors.centerIn: parent
                visible: root.checkState === Qt.PartiallyChecked
                width: IKCheckBoxTokens.partialMarkWidth
                height: IKCheckBoxTokens.partialMarkHeight
                radius: IKCheckBoxTokens.partialMarkRadius
                color: IKColors.checkboxMark
            }
        }
    }
}
