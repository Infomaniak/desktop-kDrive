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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

pragma Singleton
import QtQuick

QtObject {
    readonly property real sidebarWidth: 240
    readonly property real sidebarItemHeight: 36
    readonly property real syncSelectorHeight: 32
    readonly property real syncSelectorAdvancedHeight: 44
    readonly property real syncSelectorIconSize: 14
    readonly property real syncSelectorStatusIconSize: 14
    readonly property real syncSelectorTooltipMaxWidth: 180
    readonly property real syncSelectorTooltipPadding: IKSpacing.s8
    readonly property real syncSelectorTooltipRadius: IKRadius.r8
    readonly property int syncSelectorTooltipDelay: 250
    readonly property real errorBadgeSize: 8
    readonly property real notificationBadgeMinSize: 20
    readonly property real syncSelectorPopupMaxHeight: 260
    readonly property real homeErrorBannerHeight: 72
    readonly property real homeErrorActionButtonHeight: 24
    readonly property real homeAvatarSize: 44
    readonly property real homeDriveNameMaxWidth: 140
    readonly property real homeDriveNameMaxHeight: 40
    readonly property real homeShortcutHeight: 34
    readonly property real homeShortcutExternalIconSpacing: IKSpacing.s16
    readonly property real toolbarIconButtonSize: 36
    readonly property real toolbarSearchIconSize: 16
    readonly property real toolbarActionGroupButtonSize: 28
    readonly property real toolbarActionGroupHeight: 36
    readonly property real toolbarActionGroupPadding: 4
    readonly property real toolbarActionGroupSpacing: 4
    readonly property real toolbarActionGroupWidth: toolbarActionGroupButtonSize * 2 + toolbarActionGroupSpacing
                                                     + toolbarActionGroupPadding * 2
}
