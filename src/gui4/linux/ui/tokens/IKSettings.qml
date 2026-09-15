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
    readonly property real windowWidth: 800
    readonly property real windowHeight: 540
    readonly property real minimumWidth: 760
    readonly property real minimumHeight: 440
    readonly property real sidebarWidth: 224
    readonly property real pageMargin: 20
    readonly property real groupSpacing: 10
    readonly property real groupPadding: 10
    readonly property real rowHeight: 42
    readonly property real rowPadding: 8
    readonly property real rowSpacing: 16
    readonly property real buttonHeight: 28
    readonly property real iconButtonSize: 28
    readonly property real informationIconSize: 14
    readonly property real switchWidth: 36
    readonly property real switchHeight: 20
    readonly property real switchInset: 2
    readonly property real comboWidth: 150
    readonly property real informationDialogWidth: 454
    readonly property real dialogChromeHeight: 180
    readonly property real logoSize: 64
}
