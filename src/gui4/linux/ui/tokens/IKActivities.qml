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
    readonly property real pagePadding: IKSpacing.s24
    readonly property real sectionSpacing: IKSpacing.s32
    readonly property real headerHeight: 24
    readonly property real filterButtonHeight: 24
    readonly property real filterIconSize: 12
    // Matches the default ItemDelegate padding of the Fusion style, now set explicitly so the menu can size itself.
    readonly property real filterMenuOptionPadding: 6
    readonly property real filterMenuOptionHeight: 24
    readonly property real tableHeaderHeight: 28
    readonly property real rowHeight: 36
    readonly property real rowRadius: IKRadius.r8
    readonly property real primaryTextLineHeight: 16
    readonly property real nameColumnMinWidth: 112
    readonly property real folderColumnMinWidth: 72
    readonly property real columnResizeHandleWidth: 10
    readonly property real nameColumnRatio: 0.40
    readonly property real folderColumnRatio: 0.25
    readonly property real secondaryCellPadding: IKSpacing.s8
    readonly property real fileIconSize: 16
    readonly property real activityIconSize: 16
    readonly property real sourceIconSize: 14
    readonly property real optionsButtonSize: 24
    readonly property real optionsIconSize: 14
    readonly property real actionMenuWidth: 250
    readonly property real actionMenuItemHeight: 24
    readonly property real actionMenuIconSlotSize: 16
    readonly property real actionMenuSeparatorHeight: 11
    readonly property real progressStrokeWidth: 2
    readonly property real noActivityIllustrationWidth: 183
    readonly property real noActivityIllustrationHeight: 120.094
    readonly property real emptyContentSpacing: IKSpacing.s32
    readonly property real emptyTextSpacing: IKSpacing.s8
    readonly property real emptyTitleSize: 15

    // Errors page (Figma 5269-83680)
    readonly property real errorsHeaderButtonHeight: 24
    readonly property real errorsSectionTitleSize: IKFonts.title3Size
    readonly property real errorsSectionTitleSpacing: IKSpacing.s8
    readonly property real errorsCardSpacing: IKSpacing.s8
    readonly property real errorsCardRadius: IKRadius.r8
    readonly property real errorsCardHorizontalPadding: IKSpacing.s12
    readonly property real errorsCardVerticalPadding: IKSpacing.s8
    readonly property real errorsCardContentSpacing: IKSpacing.s8
    readonly property real errorsCardActionSpacing: IKSpacing.s16
    readonly property real errorsActionButtonHeight: 24
    readonly property real errorsChipHeight: 24
    readonly property real errorsChipRadius: IKRadius.r6
    readonly property real errorsChipHorizontalPadding: IKSpacing.s4
    readonly property real errorsChipIconSize: 12
    readonly property real errorsChipSpacing: 6
    readonly property real errorsChipMaximumWidthRatio: 0.7
    readonly property real errorsCopyFieldHeight: 28
    readonly property real errorsCopyFieldWrappedPadding: IKSpacing.s4
    readonly property real errorsCopyIconSize: 14
    readonly property real errorsInfoIconSize: 16
    readonly property real errorsDetailsLineSpacing: IKSpacing.s2
    readonly property real errorsDetailsHeaderPadding: IKSpacing.s4
    readonly property real errorsDetailsColumnSpacing: IKSpacing.s16
    // Below this width, the technical details show one name and value pair per row instead of two.
    readonly property real errorsDetailsTwoPairsMinWidth: 480
    readonly property int errorsDetailsChevronDuration: 150
    // Outline of the row the "Fix errors" action of an activity brought into view.
    readonly property real errorsRevealBorderWidth: 2
    readonly property int errorsRevealDuration: 2000
    // Explanation dialog ("Understand the problem").
    readonly property real errorsExplanationWidth: 532
    readonly property real errorsExplanationChromeHeight: 160
    readonly property real errorsExplanationReasonSpacing: IKSpacing.s16
    readonly property real errorsExplanationReasonPadding: IKSpacing.s8
    readonly property real errorsExplanationReasonRadius: IKRadius.r8
    readonly property real errorsExplanationReasonContentSpacing: IKSpacing.s8
    readonly property real errorsExplanationArrowSize: 12
    readonly property real errorsExplanationArrowSpacing: IKSpacing.s4
    // Version selection dialog of a conflict.
    readonly property real errorsConflictWidth: 600
    readonly property real errorsConflictCardSpacing: 20
    readonly property real errorsConflictCardPadding: IKSpacing.s16
    readonly property real errorsConflictCardRadius: IKRadius.r12
    readonly property real errorsConflictCardContentSpacing: IKSpacing.s16
    readonly property real errorsConflictCardTitleSpacing: 6
    readonly property real errorsConflictCardIconSize: 14
    readonly property real errorsConflictOpenIconSize: 16
    readonly property real errorsConflictInfoPadding: IKSpacing.s8
    readonly property real errorsConflictInfoRadius: IKRadius.r6
    readonly property real errorsConflictInfoSpacing: 6
    readonly property real errorsConflictInfoRowPadding: IKSpacing.s4
    readonly property real errorsConflictSelectHeight: 24
    readonly property real errorsConflictSelectIconSpacing: IKSpacing.s4
    readonly property real errorsConflictBadgeHorizontalPadding: IKSpacing.s4
    readonly property real errorsConflictBadgeVerticalPadding: IKSpacing.s2
    readonly property real errorsConflictBadgeRadius: IKRadius.r4
    readonly property real errorsConflictCounterHorizontalPadding: IKSpacing.s8
    readonly property real errorsConflictCounterVerticalPadding: 6
    // How long the copy button of a path shows its confirmation.
    readonly property int errorsCopiedFeedbackDuration: 3000
}
