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

pragma Singleton
import QtQuick

QtObject {
    readonly property real cardSpacing: 32
    readonly property real cardHorizontalPadding: 10
    // Same inset above the title as on its left, instead of the 16 px of the Figma.
    readonly property real headerTopPadding: 10
    readonly property real headerHeight: 58
    readonly property real rowHeight: 42
    readonly property real storageBarHeight: 16
    readonly property real storageBarRadius: 2
    readonly property real minimumSegmentRatio: 0.01
    readonly property real legendDotSize: 8
    readonly property real illustrationSize: 140
    readonly property real illustrationGraphicWidth: 112.13
    readonly property real illustrationGraphicHeight: 72.33
    readonly property real errorContentSpacing: 32
    readonly property real errorTextSpacing: 8
    readonly property real errorTextMaxWidth: 440
    readonly property real errorTitleSize: 15
    readonly property real errorLineHeight: 20
    readonly property real loadingSpinnerSize: 16
    readonly property real loadingPlaceholderWidth: 64
    readonly property real loadingPlaceholderHeight: 12
    readonly property real headerAccessorySpacing: 8
    readonly property real cartIconSize: 14
    readonly property real chipHeight: 24
    readonly property real chipPadding: 4
    readonly property real chipRadius: 4
    readonly property real chipSpacing: 6
    readonly property real chipIconSize: 12
    readonly property real chipMaxWidth: 200
    readonly property real legendLabelSpacing: 6
    readonly property real actionCardPadding: 10
    readonly property real actionContentSpacing: 8
    readonly property real actionTextSpacing: 2
    readonly property real actionIconTileSize: 26
    readonly property real actionIconTileRadius: 5
    readonly property real actionIconSize: 14
    readonly property real actionButtonHeight: 24
    readonly property real actionSubtitleLineHeight: 14
    readonly property real actionButtonSpacing: 16
    readonly property real unavailableCardPadding: 32
    readonly property int segmentAnimationDuration: 450
}
