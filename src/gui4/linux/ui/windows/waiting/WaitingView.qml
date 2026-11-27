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

import QtQuick
import kDrive.UI
import "../onboarding/animations"

Item {
    Item {
        anchors.centerIn: parent
        width: IKWaiting.animationWidth * IKOnboarding.loaderStrokeRenderScale
        height: IKWaiting.animationHeight * IKOnboarding.loaderStrokeRenderScale
        // Render the vector animation oversized for crispness, then scale it back to its display size.
        scale: 1 / IKOnboarding.loaderStrokeRenderScale
        transformOrigin: Item.Center
        // Antialiases the generated animation, drawn by the geometry renderer.
        layer.enabled: true
        layer.samples: IKOnboarding.illustrationAnimationSamples
        layer.smooth: true

        LoaderStrokeAnimation {
            anchors.fill: parent
            animations.loops: Animation.Infinite
        }
    }
}
