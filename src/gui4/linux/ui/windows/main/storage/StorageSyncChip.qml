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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Effects
import kDrive.UI

// Names a synchronization or a drive the way the synchronization selector does: the bare drive glyph for a drive or a
// classic synchronization, the folder outline for an advanced synchronization, both tinted with the drive color.
Rectangle {
    id: root

    required property string title
    property color driveColor: IKColors.driveDefaultColor
    property bool advancedSync: false

    readonly property real maximumLabelWidth: IKStorage.chipMaxWidth - 2 * IKStorage.chipPadding - IKStorage.chipIconSize
                                              - IKStorage.chipSpacing

    color: IKColors.storageChipSurface
    radius: IKStorage.chipRadius
    implicitWidth: content.implicitWidth + 2 * IKStorage.chipPadding
    implicitHeight: IKStorage.chipHeight

    Row {
        id: content

        anchors.left: parent.left
        anchors.leftMargin: IKStorage.chipPadding
        anchors.verticalCenter: parent.verticalCenter
        spacing: IKStorage.chipSpacing

        Item {
            anchors.verticalCenter: parent.verticalCenter
            width: IKStorage.chipIconSize
            height: IKStorage.chipIconSize

            IKDriveIcon {
                anchors.fill: parent
                visible: !root.advancedSync
                driveColor: root.driveColor
            }

            Image {
                anchors.fill: parent
                visible: root.advancedSync
                source: "qrc:/assets/main/folder.svg"
                sourceSize.width: Math.ceil(width * Math.max(3, Screen.devicePixelRatio))
                fillMode: Image.PreserveAspectFit
                smooth: true
                layer.enabled: visible
                layer.effect: MultiEffect {
                    colorization: 1
                    colorizationColor: root.driveColor
                }
            }
        }

        Text {
            id: label

            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, root.maximumLabelWidth)
            text: root.title
            textFormat: Text.PlainText
            color: IKColors.textPrimary
            font.pixelSize: IKFonts.bodySize
            elide: Text.ElideRight
        }
    }

    HoverHandler {
        id: chipHover
    }

    IKToolTip {
        showRequested: chipHover.hovered && label.truncated
        text: root.title
    }
}
