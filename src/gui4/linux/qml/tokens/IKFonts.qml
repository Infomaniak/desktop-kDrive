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

pragma Singleton
import QtQuick

QtObject {
    // Sizes (in pt)
    readonly property real largeTitleSize: 26
    readonly property real titleSize:      22
    readonly property real title2Size:     20
    readonly property real title3Size:     17
    readonly property real headlineSize:   14
    readonly property real bodySize:       13
    readonly property real subheadlineSize: 11

    // Weights
    readonly property int regular:    Font.Normal
    readonly property int emphasized: Font.DemiBold
}