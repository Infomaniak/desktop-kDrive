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

// Search dialog geometry, copied from the macOS search sheet.
QtObject {
    readonly property real dialogWidth: 600
    readonly property real dialogHeight: 368

    readonly property real fieldHeight: 36
    readonly property real fieldRadius: IKRadius.r16
    readonly property real fieldMargin: IKSpacing.s16
    readonly property real fieldHorizontalPadding: IKSpacing.s12
    readonly property real fieldSpacing: IKSpacing.s8
    readonly property real fieldIconSize: IKIconSizes.medium
    // Cross drawn inside the clear button disc, like the macOS xmark.circle.fill symbol.
    readonly property real clearGlyphLength: 8
    readonly property real clearGlyphThickness: 1.5

    readonly property real listHorizontalMargin: IKSpacing.s16
    readonly property real rowPadding: IKSpacing.s8
    readonly property real rowSpacing: IKSpacing.s8
    readonly property real rowRadius: IKRadius.r4
    readonly property real rowFileIconSize: IKIconSizes.small
    readonly property real rowTextLineHeight: 20
    readonly property real rowSubtitleLineHeight: 14

    readonly property int placeholderRowCount: 5
    readonly property real placeholderNameWidthRatio: 0.45
    readonly property real placeholderSubtitleWidthRatio: 0.7
    readonly property real placeholderBarRadius: IKRadius.r4
    // Height of a skeleton text bar relative to the font size of the text it stands for.
    readonly property real placeholderTextBarHeightRatio: 0.8
    readonly property int placeholderPulseDuration: 900
    readonly property real placeholderMinimumOpacity: 0.45

    // A next page is requested once fewer loaded rows than this remain below the viewport: with pages of 50 results,
    // around the middle of the last page, which leaves time for the response to arrive.
    readonly property int loadMoreRemainingRows: 20

    readonly property real footerHeight: 44

    readonly property real emptyIllustrationMaxWidth: 200
    readonly property real emptyIllustrationAspectRatio: 184 / 122
    readonly property real emptyContentSpacing: IKSpacing.s32
    readonly property real emptyTextSpacing: IKSpacing.s8
    readonly property real emptyContentMargin: IKSpacing.s24
}
