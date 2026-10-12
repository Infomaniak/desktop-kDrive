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
    // Shared blocking-state card (IKBlockingState), from the update-required and blocking-error designs.
    readonly property real cardRadius: IKRadius.r16
    readonly property real cardBorderWidth: 1
    readonly property real contentMargin: IKSpacing.s24
    readonly property real visualSpacing: IKSpacing.s32
    readonly property real textSpacing: IKSpacing.s16
    readonly property real textToActionsSpacing: IKSpacing.s24
    readonly property real actionsSpacing: IKSpacing.s8
    readonly property real textMaximumWidth: 520

    // Update-required screen, which replaces the whole window content.
    // The window header (IKWindow.headerHeight) already separates the card from the top edge.
    readonly property real updateRequiredTopMargin: 0
    readonly property real updateRequiredMargin: IKSpacing.page
    readonly property real updateRequiredIllustrationWidth: 178
    readonly property real updateRequiredIllustrationHeight: 131
}
