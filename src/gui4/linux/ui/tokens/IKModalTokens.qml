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
    readonly property real width: 480
    readonly property real screenMargin: IKSpacing.s32
    readonly property real contentPadding: IKSpacing.s24
    readonly property real contentSpacing: IKSpacing.s24
    readonly property real bodySpacing: IKSpacing.s16
    readonly property real headerSpacing: IKSpacing.s12
    readonly property real actionSpacing: IKSpacing.s8
    readonly property real iconSize: 24
    readonly property real buttonHeight: 36
    readonly property real buttonMinimumWidth: 104
    readonly property real buttonHorizontalPadding: IKSpacing.s16
    readonly property real buttonSpinnerSize: 16
    readonly property real checkboxSize: 18
    readonly property real errorIconSize: 18
    readonly property real errorPadding: IKSpacing.s12
    readonly property real borderWidth: 1
    readonly property real focusBorderWidth: 2
    readonly property real disabledOpacity: 0.55
    readonly property real hoverOpacity: 0.92
    readonly property real pressedOpacity: 0.82
    readonly property real enterScale: 0.97
    readonly property int enterDuration: 140
    readonly property int exitDuration: 100
}
