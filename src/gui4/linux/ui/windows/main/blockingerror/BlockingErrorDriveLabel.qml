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

// Drive of the blocked synchronization, with a status badge overlapping its top-right corner.
Rectangle {
    id: root

    required property string driveName
    required property color driveColor
    required property url badgeSource
    required property color badgeSurfaceColor
    required property color badgeColor

    implicitWidth: content.implicitWidth + 2 * IKBlockingStateTokens.driveLabelHorizontalPadding
    implicitHeight: content.implicitHeight + 2 * IKBlockingStateTokens.driveLabelVerticalPadding
    width: implicitWidth
    height: implicitHeight
    radius: IKBlockingStateTokens.driveLabelRadius
    color: IKColors.blockingErrorDriveLabelSurface
    border.width: 1
    border.color: IKColors.blockingErrorDriveLabelBorder

    Row {
        id: content

        anchors.centerIn: parent
        spacing: IKBlockingStateTokens.driveLabelSpacing

        IKDriveIcon {
            anchors.verticalCenter: parent.verticalCenter
            width: IKBlockingStateTokens.driveLabelIconSize
            height: IKBlockingStateTokens.driveLabelIconSize
            driveColor: root.driveColor
        }

        Text {
            id: driveNameText

            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, IKBlockingStateTokens.driveLabelMaximumTextWidth)
            text: root.driveName
            textFormat: Text.PlainText
            elide: Text.ElideRight
            color: IKColors.textSecondary
            font.pixelSize: IKBlockingStateTokens.driveLabelTextSize
        }
    }

    HoverHandler {
        id: labelHover
    }

    IKToolTip {
        showRequested: labelHover.hovered && driveNameText.truncated
        text: root.driveName
    }

    Rectangle {
        x: root.width - width / 2
        y: -height / 2
        width: IKBlockingStateTokens.badgeSize
        height: IKBlockingStateTokens.badgeSize
        radius: width / 2
        color: root.badgeSurfaceColor
        border.width: 1
        border.color: root.badgeColor

        IKTintedIcon {
            anchors.centerIn: parent
            width: IKBlockingStateTokens.badgeIconSize
            height: IKBlockingStateTokens.badgeIconSize
            source: root.badgeSource
            color: root.badgeColor
        }
    }
}
