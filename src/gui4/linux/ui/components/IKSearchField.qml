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
import kDrive.UI

// Rounded search field: magnifier, borderless text input, and a clear button shown with text. The owner keeps its query
// in sync through `text`, `textEdited` and `cleared`.
Rectangle {
    id: root

    property alias text: input.text
    property alias placeholderText: input.placeholderText
    property string accessibleName: qsTrId("buttonSearch")

    // Typed by the user, unlike a change of `text` by the owner.
    signal textEdited(string editedText)
    // The clear button emptied the field.
    signal cleared
    signal downPressed

    function focusInput() {
        input.forceActiveFocus();
    }

    implicitHeight: IKSearch.fieldHeight
    radius: IKSearch.fieldRadius
    color: IKColors.surfaceSecondary

    IKTintedIcon {
        id: searchIcon

        anchors.left: parent.left
        anchors.leftMargin: IKSearch.fieldHorizontalPadding
        anchors.verticalCenter: parent.verticalCenter
        width: IKSearch.fieldIconSize
        height: IKSearch.fieldIconSize
        source: "qrc:/assets/main/home/search.svg"
        color: IKColors.textSecondary
    }

    TextField {
        id: input

        anchors.left: searchIcon.right
        anchors.leftMargin: IKSearch.fieldSpacing
        anchors.right: clearButton.visible ? clearButton.left : parent.right
        anchors.rightMargin: clearButton.visible ? IKSearch.fieldSpacing : IKSearch.fieldHorizontalPadding
        anchors.verticalCenter: parent.verticalCenter
        padding: 0
        background: null
        color: IKColors.textPrimary
        placeholderTextColor: IKColors.textTertiary
        font.pixelSize: IKFonts.bodySize
        selectByMouse: true
        Accessible.name: root.accessibleName

        onTextEdited: root.textEdited(text)
        Keys.onDownPressed: root.downPressed()
    }

    ToolButton {
        id: clearButton

        anchors.right: parent.right
        anchors.rightMargin: IKSearch.fieldHorizontalPadding
        anchors.verticalCenter: parent.verticalCenter
        width: IKSearch.fieldIconSize
        height: IKSearch.fieldIconSize
        padding: 0
        visible: input.text.length > 0
        focusPolicy: Qt.NoFocus
        hoverEnabled: true
        display: AbstractButton.IconOnly
        text: qsTrId("accessibilitySearchClear")
        background: null

        // Drawn with rectangles rather than a tinted SVG: the scene graph renders them sharp at any scale, while the
        // diagonal cut-out of a 16 px SVG blurs once rasterized.
        contentItem: Item {
            Rectangle {
                anchors.centerIn: parent
                width: IKSearch.fieldIconSize
                height: IKSearch.fieldIconSize
                radius: width / 2
                color: clearButton.hovered ? IKColors.textSecondary : IKColors.textTertiary
                antialiasing: true

                Repeater {
                    model: [45, -45]

                    Rectangle {
                        required property int modelData

                        anchors.centerIn: parent
                        width: IKSearch.clearGlyphLength
                        height: IKSearch.clearGlyphThickness
                        radius: height / 2
                        rotation: modelData
                        color: root.color
                        antialiasing: true
                    }
                }
            }
        }

        onClicked: {
            input.clear();
            root.cleared();
            input.forceActiveFocus();
        }

        IKToolTip {
            targetButton: clearButton
            text: clearButton.text
        }
    }
}
