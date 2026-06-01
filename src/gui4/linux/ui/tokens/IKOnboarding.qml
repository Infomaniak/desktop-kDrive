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
    // Splits the onboarding window between the form area and the illustration area.
    readonly property real contentPanelWidthRatio: 0.63
    readonly property real illustrationPanelWidthRatio: 0.37

    // Leaves breathing room around the Lottie animation inside the illustration panel.
    readonly property real illustrationAnimationMaxSize: 260
    readonly property real illustrationAnimationFillRatio: 0.8

    // Source dimensions of the loader-stroke animation.
    readonly property int loaderStrokeAnimationWidth: 302
    readonly property int loaderStrokeAnimationHeight: 260
    readonly property real loaderStrokeVectorSourceWidth: 150.83
    readonly property real loaderStrokeVectorSourceHeight: 129.8
    readonly property real loaderStrokeAnimationHeightRatio: loaderStrokeAnimationHeight / loaderStrokeAnimationWidth
}
