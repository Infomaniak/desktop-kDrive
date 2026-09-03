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
    readonly property real indicatorSize: 16
    readonly property real indicatorRadius: 5
    readonly property real borderWidth: 1
    readonly property real focusRingWidth: 2
    readonly property real focusRingRadius: IKRadius.r8
    // Pointer and keyboard target around the smaller drawn indicator.
    readonly property real hitAreaSize: 24
    // Coordinate space the check mark path is authored in; scaled to indicatorSize at paint time.
    readonly property real markReferenceSize: 16
    readonly property real markStrokeWidth: 1.6
    readonly property real partialMarkWidth: 8
    readonly property real partialMarkHeight: 2
    readonly property real partialMarkRadius: 1
    readonly property real disabledOpacity: 0.45
}
