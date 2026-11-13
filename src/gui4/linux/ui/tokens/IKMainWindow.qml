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
    readonly property int tooltipDelay: 400
    readonly property real errorBadgeSize: 8
    readonly property real notificationBadgeMinSize: 20
    readonly property real syncSelectorPopupMaxHeight: 260
    readonly property real sidebarNotificationMinHeight: 40
    readonly property real sidebarNotificationIconSize: 14
    readonly property int sidebarNotificationDuration: 5000
    // Information blocks: one action is 64 px high, an action with its dismiss button below is 80 px.
    readonly property real informationBlockMinHeight: 64
    readonly property real informationBlockActionButtonHeight: 24
    readonly property real informationBlockIconTileSize: 26
    readonly property real informationBlockIconTileRadius: 5
    readonly property real informationBlockIconSize: 14
    readonly property real informationBlockExternalIconSpacing: IKSpacing.s8
    readonly property real homeAvatarSize: 44
    readonly property real homeAvatarSettingsIconSize: 16
    // The synchronization badge sits on the bottom-right of the avatar and overflows it.
    readonly property real homeSyncBadgeSize: 20
    readonly property real homeSyncBadgeGlyphSize: 12
    readonly property real homeSyncBadgeSettingsIconSize: 10
    readonly property real homeSyncBadgeOverflowX: 4
    readonly property real homeSyncBadgeOverflowY: 6
    readonly property real homeDriveNameMaxWidth: 140
    readonly property real homeDriveNameMaxHeight: 40
    readonly property real homeShortcutHeight: 34
    readonly property real homeShortcutExternalIconSpacing: IKSpacing.s16
    // Fixed canvas so the Home layout does not shift when the status visual changes.
    readonly property real homeStatusVisualWidth: 228
    readonly property real homeStatusVisualHeight: 131
    readonly property real homeStatusAnimationWidth: 178
    readonly property real homeStatusAnimationHeight: 131
    readonly property real homeLoadingSpinnerSize: 32
    readonly property real homeStatusOfflineAnimationWidth: 227
    readonly property real homeStatusOfflineAnimationHeight: 90
    // Multisampling of the status animations: the geometry renderer draws their paths without antialiasing.
    readonly property int homeStatusAnimationSamples: 4
    readonly property real homeStatusSetupImageWidth: 126
    readonly property real homeStatusSetupImageHeight: 121
    readonly property real toolbarIconButtonSize: 36
    readonly property real toolbarSearchIconSize: 16
    readonly property real toolbarActionGroupButtonSize: 28
    readonly property real toolbarActionGroupHeight: 36
    readonly property real toolbarActionGroupPadding: 4
    readonly property real toolbarActionGroupSpacing: 4
    readonly property real toolbarActionGroupWidth: toolbarActionGroupButtonSize * 2 + toolbarActionGroupSpacing
                                                     + toolbarActionGroupPadding * 2
    // With several synchronizations, a chevron that opens the menu follows Pause/Resume, separated from Settings.
    readonly property real toolbarSyncMenuChevronButtonWidth: 16
    readonly property real toolbarSyncMenuChevronSize: 12
    readonly property real toolbarActionGroupSeparatorSlotWidth: 3
    readonly property real toolbarActionGroupSeparatorHeight: 20
    readonly property real toolbarActionGroupMenuWidth: toolbarActionGroupPadding * 2 + toolbarActionGroupButtonSize * 2
                                                         + toolbarSyncMenuChevronButtonWidth
                                                         + toolbarActionGroupSpacing * 2
                                                         + toolbarActionGroupSeparatorSlotWidth
    // Pause/Resume menu, opened below the toolbar group.
    readonly property real syncControlMenuWidth: 250
    readonly property real syncControlMenuOffset: 4
    readonly property real syncControlMenuItemHeight: 24
    readonly property real syncControlMenuSeparatorHeight: 11
    readonly property real syncControlMenuIconSize: 12
    readonly property real syncControlMenuIconSpacing: 6

    // Single source of the selector row height, shared by the row and the popup that sizes itself on its rows.
    function syncSelectorItemHeight(hasSubtitle: bool): real {
        return hasSubtitle ? syncSelectorAdvancedHeight : syncSelectorHeight
    }
}
