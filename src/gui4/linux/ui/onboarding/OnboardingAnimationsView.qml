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

    readonly property real contentHeightRatio: sourceHeight / sourceWidth
    required property var onboardingFlowController
    readonly property bool showSyncFile: root.onboardingFlowController.driveSelectionActive || root.onboardingFlowController.synchronizationActive
    readonly property real sourceHeight: showSyncFile ? IKOnboarding.syncFileVectorSourceHeight : IKOnboarding.loaderStrokeVectorSourceHeight

    // Native source dimensions of the currently displayed animation. Exposed so the
    // illustration panel can preserve the animation aspect ratio across steps.
    readonly property real sourceWidth: showSyncFile ? IKOnboarding.syncFileVectorSourceWidth : IKOnboarding.loaderStrokeVectorSourceWidth

    Item {
        id: animationFrame

        anchors.centerIn: parent
        height: root.sourceHeight * IKOnboarding.loaderStrokeRenderScale
        scale: Math.min(root.width / width, root.height / height)
        transformOrigin: Item.Center
        // Render the vector animation oversized for crispness, then scale it to fit the view.
        width: root.sourceWidth * IKOnboarding.loaderStrokeRenderScale

        Loader {
            anchors.fill: parent
            sourceComponent: root.showSyncFile ? (ThemeMode.isDark ? syncFileDarkComponent : syncFileLightComponent) : loaderStrokeComponent
        }
    }
    Component {
        id: loaderStrokeComponent

        LoaderStrokeAnimation {
            anchors.fill: parent
            animations.loops: Animation.Infinite
        }
    }
    Component {
        id: syncFileLightComponent

        SyncFileLightAnimation {
            anchors.fill: parent
            animations.loops: Animation.Infinite
        }
    }
    Component {
        id: syncFileDarkComponent

        SyncFileDarkAnimation {
            anchors.fill: parent
            animations.loops: Animation.Infinite
        }
    }
}
