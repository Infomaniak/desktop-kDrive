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
    readonly property real cardHeight: 190
    readonly property real cardHorizontalPadding: 10
    readonly property real headerHeight: 64
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
    readonly property real retryHeight: 20
    readonly property real loadingSpinnerSize: 16
    readonly property real loadingPlaceholderWidth: 64
    readonly property real loadingPlaceholderHeight: 12
}
