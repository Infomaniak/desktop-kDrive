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

Row {
    id: root

    property string label: ""
    property string subtitle: ""
    property string avatarSource: ""

    spacing: IKSpacing.s8

    Rectangle {
        width: IKOnboarding.driveSelectionUserAvatarSize
        height: IKOnboarding.driveSelectionUserAvatarSize
        radius: IKOnboarding.driveSelectionUserBadgeRadius
        color: IKColors.onboardingUserBadgeSurface
        clip: true

        Image {
            anchors.fill: parent
            visible: root.avatarSource.length > 0
            source: root.avatarSource
            fillMode: Image.PreserveAspectCrop
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
            color: IKColors.textPrimary
            font.pixelSize: IKFonts.bodySize
            lineHeightMode: Text.FixedHeight
            lineHeight: IKOnboarding.driveSelectionDriveNameLineHeight
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
