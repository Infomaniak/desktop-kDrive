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
import "animations"

Item {
    id: root

    Item {
        id: animationFrame

        anchors.centerIn: parent
        width: IKOnboarding.loaderStrokeAnimationWidth
        height: IKOnboarding.loaderStrokeAnimationHeight
        scale: Math.min(root.width / width, root.height / height)
        transformOrigin: Item.Center

        Loader {
            id: loader

            anchors.centerIn: parent
            width: IKOnboarding.loaderStrokeVectorSourceWidth
            height: IKOnboarding.loaderStrokeVectorSourceHeight
            scale: Math.min(animationFrame.width / width, animationFrame.height / height)
            transformOrigin: Item.Center
            sourceComponent: ThemeMode.isDark ? loaderStrokeDarkComponent : loaderStrokeLightComponent
        }
    }

    Component {
        id: loaderStrokeDarkComponent

        LoaderStrokeDarkAnimation {
            animations.loops: Animation.Infinite
        }
    }

    Component {
        id: loaderStrokeLightComponent

        LoaderStrokeLightAnimation {
            animations.loops: Animation.Infinite
        }
    }
}
