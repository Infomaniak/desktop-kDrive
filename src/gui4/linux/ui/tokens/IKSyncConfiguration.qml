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
    // Modal shell
    readonly property real modalWidth: 600
    readonly property real sectionSpacing: IKSpacing.s16
    readonly property real tooltipMaximumWidth: 320

    // Selected-drive summary
    readonly property real summaryDescriptionWidth: 430
    readonly property real summaryListMaximumHeight: 276
    readonly property real summaryListSpacing: IKSpacing.s8
    readonly property real summaryRowRadius: IKRadius.r8
    readonly property real summaryRowPadding: IKSpacing.s16
    readonly property real summaryRowSpacing: IKSpacing.s12
    readonly property real summaryRowTextSpacing: IKSpacing.s2

    // Per-drive configuration
    readonly property real driveIconSize: 20
    readonly property real driveBadgeRadius: IKRadius.r8
    readonly property real driveBadgePadding: IKSpacing.s8
    readonly property real driveBadgeSpacing: IKSpacing.s8
    readonly property real cardRadius: IKRadius.r8
    readonly property real cardPadding: IKSpacing.s16
    readonly property real cardSpacing: IKSpacing.s12
    readonly property real cardTitleSpacing: IKSpacing.s4
    readonly property real fieldSpacing: IKSpacing.s8
    readonly property real fieldRadius: IKRadius.r6
    readonly property real fieldBorderWidth: 1
    readonly property real fieldHorizontalPadding: IKSpacing.s8
    readonly property real fieldVerticalPadding: IKSpacing.s4
    readonly property real folderIconSize: 16
    readonly property real statusIconSize: 14
    readonly property real statusSpacing: IKSpacing.s8

    // Remote folder tree
    readonly property real treeHeight: 330
    readonly property real treeRadius: IKRadius.r8
    readonly property real treeBorderWidth: 1
    readonly property real treeHeaderHeight: 32
    readonly property real treeRowHeight: 32
    readonly property real treeRowRadius: IKRadius.r4
    readonly property real treeContentPadding: IKSpacing.s4
    readonly property real treeIndent: 20
    readonly property real treeDisclosureSize: 20
    readonly property real treeChevronSize: 10
    readonly property real treeRowIconSize: 16
    readonly property real treeRowSpacing: IKSpacing.s8
    readonly property real treeSizeColumnWidth: 88
    readonly property real treeStateSpacing: IKSpacing.s12
    readonly property real treeSpinnerSize: 12
}
