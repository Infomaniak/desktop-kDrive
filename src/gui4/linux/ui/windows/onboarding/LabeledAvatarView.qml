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
import QtQuick.Window
import kDrive.UI

Row {
    id: root

    property string label: ""
    property string subtitle: ""
    property string avatarSource: ""
    property color avatarMaskColor: IKColors.onboardingUserBadgeSurface

    spacing: IKOnboarding.driveSelectionUserBadgeContentSpacing

    onAvatarMaskColorChanged: avatarClipOverlay.requestPaint()

    Item {
        id: avatarContainer

        width: IKOnboarding.driveSelectionUserAvatarSize
        height: IKOnboarding.driveSelectionUserAvatarSize

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: IKColors.surfacePrimary
        }

        Image {
            id: avatarImage

            anchors.fill: parent
            visible: root.avatarSource.length > 0
            source: root.avatarSource
            fillMode: Image.PreserveAspectCrop
            smooth: true
            mipmap: true
            asynchronous: true
            sourceSize.width: Math.round(width * Screen.devicePixelRatio)
            sourceSize.height: Math.round(height * Screen.devicePixelRatio)
        }

        Canvas {
            id: avatarClipOverlay

            anchors.fill: parent
            visible: root.avatarSource.length > 0
            antialiasing: true
            onPaint: {
                const context = getContext("2d")
                context.clearRect(0, 0, width, height)
                context.fillStyle = root.avatarMaskColor
                context.fillRect(0, 0, width, height)
                context.globalCompositeOperation = "destination-out"
                context.beginPath()
                context.arc(width / 2, height / 2, width / 2, 0, 2 * Math.PI)
                context.fill()
                context.globalCompositeOperation = "source-over"
            }
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
        }

        Text {
            anchors.centerIn: parent
            visible: root.avatarSource.length === 0
            text: root.label.length > 0 ? root.label.charAt(0).toUpperCase() : ""
            color: IKColors.textSecondary
            font.pixelSize: IKFonts.subheadlineSize
            font.weight: IKFonts.emphasized
        }
    }

    Column {
        width: Math.max(0, root.width - IKOnboarding.driveSelectionUserAvatarSize - root.spacing)
        anchors.verticalCenter: parent.verticalCenter
        spacing: 0

        Text {
            width: parent.width
            text: root.label
            color: IKColors.textSecondary
            font.pixelSize: IKFonts.headlineSize
            lineHeightMode: Text.FixedHeight
            lineHeight: IKOnboarding.driveSelectionUserNameLineHeight
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            visible: root.subtitle.length > 0
            text: root.subtitle
            color: IKColors.textSecondary
            font.pixelSize: IKFonts.subheadlineSize
            lineHeightMode: Text.FixedHeight
            lineHeight: IKOnboarding.driveSelectionAccountLineHeight
            elide: Text.ElideRight
        }
    }
}
