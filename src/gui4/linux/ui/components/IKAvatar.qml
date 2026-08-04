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

import QtQuick
import QtQuick.Shapes
import QtQuick.Window
import kDrive.UI

Item {
    id: root

    property string source: ""
    property string fallbackLabel: ""
    property color maskColor: IKColors.surfaceSecondary
    property color borderColor: IKColors.accentPrimary
    property real borderWidth: 1
    readonly property real sourceScale: Math.max(2, Screen.devicePixelRatio)

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: IKColors.surfacePrimary
    }

    Image {
        id: avatarImage

        anchors.fill: parent
        anchors.margins: root.borderWidth
        visible: root.source.length > 0
        source: root.source
        fillMode: Image.PreserveAspectCrop
        smooth: true
        mipmap: true
        asynchronous: true
        sourceSize.width: Math.round(width * root.sourceScale)
        sourceSize.height: Math.round(height * root.sourceScale)
    }

    /*
     * Keep the Image rectangular so Qt can preserve DPR-aware downscaling, smoothing, and mipmapping. This vector
     * overlay hides its corners instead: the PathMove/PathLine elements draw the outer rectangle, the two PathArc
     * elements draw the circular opening, and OddEvenFill turns that inner contour into a transparent hole.
     */
    Shape {
        id: avatarClipOverlay

        anchors.fill: parent
        visible: root.source.length > 0
        preferredRendererType: Shape.CurveRenderer
        antialiasing: true

        ShapePath {
            id: avatarClipPath

            readonly property real clipRadius: Math.max(0, avatarImage.width / 2)

            strokeColor: "transparent"
            fillColor: root.maskColor
            fillRule: ShapePath.OddEvenFill

            PathMove {
                x: 0
                y: 0
            }
            PathLine {
                x: avatarClipOverlay.width
                y: 0
            }
            PathLine {
                x: avatarClipOverlay.width
                y: avatarClipOverlay.height
            }
            PathLine {
                x: 0
                y: avatarClipOverlay.height
            }
            PathLine {
                x: 0
                y: 0
            }

            PathMove {
                x: avatarClipOverlay.width / 2
                y: avatarClipOverlay.height / 2 - avatarClipPath.clipRadius
            }
            PathArc {
                x: avatarClipOverlay.width / 2
                y: avatarClipOverlay.height / 2 + avatarClipPath.clipRadius
                radiusX: avatarClipPath.clipRadius
                radiusY: avatarClipPath.clipRadius
            }
            PathArc {
                x: avatarClipOverlay.width / 2
                y: avatarClipOverlay.height / 2 - avatarClipPath.clipRadius
                radiusX: avatarClipPath.clipRadius
                radiusY: avatarClipPath.clipRadius
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: "transparent"
        border.width: root.borderWidth
        border.color: root.borderColor
    }

    Text {
        anchors.centerIn: parent
        visible: root.source.length === 0
        text: root.fallbackLabel.length > 0 ? root.fallbackLabel.charAt(0).toUpperCase() : ""
        color: IKColors.textSecondary
        font.pixelSize: IKFonts.headlineSize
        font.weight: IKFonts.emphasized
    }
}
