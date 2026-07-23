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
import kDrive.UI

// Displays either a compact status dot or a numeric notification badge.
Rectangle {
    id: root

    property int count: 0
    property bool dot: false

    readonly property bool showCount: count > 0

    visible: showCount || dot
    implicitWidth: showCount ? Math.max(IKMainWindow.notificationBadgeMinSize,
                                       countLabel.implicitWidth + IKSpacing.s8) : IKMainWindow.errorBadgeSize
    implicitHeight: showCount ? IKMainWindow.notificationBadgeMinSize : IKMainWindow.errorBadgeSize
    radius: height / 2
    color: IKColors.actionPrimary

    Text {
        id: countLabel

        anchors.centerIn: parent
        visible: root.showCount
        text: root.count
        color: IKColors.actionOnPrimary
        font.pixelSize: IKFonts.subheadlineSize
        font.weight: IKFonts.emphasized
    }
}
