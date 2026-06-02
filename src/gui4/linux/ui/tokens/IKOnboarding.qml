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

    // Splits the onboarding window between the form area and the illustration area.
    readonly property real contentPanelWidthRatio: 0.63
    readonly property real illustrationPanelWidthRatio: 1 - contentPanelWidthRatio

    // Leaves breathing room around the Lottie animation inside the illustration panel.
    readonly property real illustrationAnimationMaxSize: loaderStrokeAnimationHeight
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

    // Source dimensions of the loader-stroke animation.
    readonly property int loaderStrokeAnimationWidth: 302
    readonly property int loaderStrokeAnimationHeight: 260
    readonly property real loaderStrokeVectorSourceWidth: 150.83
    readonly property real loaderStrokeVectorSourceHeight: 129.8
    readonly property real loaderStrokeAnimationHeightRatio: loaderStrokeAnimationHeight / loaderStrokeAnimationWidth
}
