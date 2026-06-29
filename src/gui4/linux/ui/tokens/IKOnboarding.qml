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
    // Local primitives.
    readonly property real contentMaxWidth: 378
    readonly property real compactBreakpointWidth: 520
    readonly property real expandedContentLeftMargin: 80
    readonly property real compactButtonHeight: 24
    readonly property real largeButtonHeight: 36
    readonly property real buttonCornerRadius: 6
    readonly property real titleLineHeight: 32
    readonly property real bodyLineHeight: 16
    readonly property real browserTitleSize: 28
    readonly property real browserTitleLineHeight: 36
    readonly property real browserBodySize: IKFonts.headlineSize
    readonly property real browserBodyLineHeight: IKFonts.title2Size
    readonly property real primaryButtonMinWidth: 105
    readonly property real secondaryButtonMinWidth: 128
    readonly property real browserButtonMinWidth: 190
    readonly property real loaderStrokeDisplayMaxSize: 260
    readonly property real loaderStrokeRenderScale: 2

    // Splits the onboarding window between the form area and the illustration area.
    readonly property real contentPanelWidthRatio: 0.63
    readonly property real illustrationPanelWidthRatio: 1 - contentPanelWidthRatio

    // Leaves breathing room around the Lottie animation inside the illustration panel.
    readonly property real illustrationAnimationMaxSize: loaderStrokeDisplayMaxSize
    readonly property real illustrationAnimationFillRatio: 0.8

    // Login screen layout.
    readonly property real loginCompactBreakpointWidth: compactBreakpointWidth
    readonly property real loginContentMaxWidth: contentMaxWidth
    readonly property real loginContentExpandedLeftMargin: expandedContentLeftMargin
    readonly property real loginButtonSpacing: IKSpacing.s8
    readonly property real loginButtonHeight: compactButtonHeight
    readonly property real loginButtonCornerRadius: buttonCornerRadius
    readonly property real loginCreateAccountButtonMinWidth: secondaryButtonMinWidth
    readonly property real loginButtonMinWidth: primaryButtonMinWidth
    readonly property real loginTitleLineHeight: titleLineHeight
    readonly property real loginBodyLineHeight: bodyLineHeight
    readonly property real loginBrowserContentSpacing: IKSpacing.s24
    readonly property real loginBrowserTextSpacing: IKSpacing.s12
    readonly property real loginBrowserTitleSize: browserTitleSize
    readonly property real loginBrowserTitleLineHeight: browserTitleLineHeight
    readonly property real loginBrowserBodySize: browserBodySize
    readonly property real loginBrowserBodyLineHeight: browserBodyLineHeight
    readonly property real loginBrowserButtonHeight: largeButtonHeight
    readonly property real loginBrowserButtonMinWidth: browserButtonMinWidth

    // Drive selection screen layout.
    readonly property real driveSelectionCompactBreakpointWidth: compactBreakpointWidth
    readonly property real driveSelectionContentMaxWidth: contentMaxWidth
    readonly property real driveSelectionContentExpandedLeftMargin: expandedContentLeftMargin
    readonly property real driveSelectionTitleLineHeight: titleLineHeight
    readonly property real driveSelectionSectionSpacing: IKSpacing.s24
    readonly property real driveSelectionUserBadgeHeight: 32
    readonly property real driveSelectionUserBadgeMaxWidth: 216
    readonly property real driveSelectionUserAvatarSize: 24
    readonly property real driveSelectionUserBadgeRadius: IKRadius.r4
    readonly property real driveSelectionUserBadgeLeftPadding: 5
    readonly property real driveSelectionUserBadgeRightPadding: 7
    readonly property real driveSelectionUserBadgeVerticalPadding: IKSpacing.s4
    readonly property real driveSelectionUserBadgeContentSpacing: 10
    readonly property real driveSelectionUserNameLineHeight: 20
    readonly property real driveSelectionListTitleLineHeight: 14
    readonly property real driveSelectionListWidth: 264
    readonly property real driveSelectionListMaxHeight: 176
    readonly property real driveSelectionCellPadding: IKSpacing.s8
    readonly property real driveSelectionCellRadius: IKRadius.r8
    readonly property real driveSelectionCellSpacing: IKSpacing.s8
    readonly property real driveSelectionCellMinHeight: 40
    readonly property real driveSelectionCheckboxSize: 16
    readonly property real driveSelectionDriveIconSize: 20
    readonly property real driveSelectionDriveIconRadius: 5
    readonly property real driveSelectionDriveIconGlyphInset: 4
    readonly property real driveSelectionDriveIconBorderWidth: 1
    readonly property real driveSelectionDriveNameLineHeight: 16
    readonly property real driveSelectionAccountLineHeight: 14
    readonly property real driveSelectionEmptyTitleLineHeight: 22
    readonly property real driveSelectionButtonHeight: compactButtonHeight
    readonly property real driveSelectionButtonMinWidth: primaryButtonMinWidth
    readonly property real driveSelectionSecondaryButtonMinWidth: secondaryButtonMinWidth
    readonly property real driveSelectionTooltipMaxWidth: 180
    readonly property real driveSelectionTooltipPadding: IKSpacing.s8
    readonly property real driveSelectionTooltipRadius: IKRadius.r8
    readonly property int driveSelectionTooltipDelay: 250

    // Source dimensions of the loader-stroke animation.
    readonly property real loaderStrokeVectorSourceWidth: 163
    readonly property real loaderStrokeVectorSourceHeight: 134
    readonly property real loaderStrokeAnimationWidth: loaderStrokeVectorSourceWidth * loaderStrokeRenderScale
    readonly property real loaderStrokeAnimationHeight: loaderStrokeVectorSourceHeight * loaderStrokeRenderScale
    readonly property real loaderStrokeAnimationHeightRatio: loaderStrokeAnimationHeight / loaderStrokeAnimationWidth
}
