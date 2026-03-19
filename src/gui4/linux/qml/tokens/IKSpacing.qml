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
    // T1 — Primitives
    readonly property real s2:  2
    readonly property real s4:  4
    readonly property real s8:  8
    readonly property real s12: 12
    readonly property real s16: 16
    readonly property real s24: 24
    readonly property real s32: 32
    readonly property real s48: 48
    readonly property real s64: 64

    // T2 — Semantic tokens
    readonly property real page: s24
}