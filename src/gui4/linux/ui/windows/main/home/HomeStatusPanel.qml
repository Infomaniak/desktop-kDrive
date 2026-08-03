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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

pragma ComponentBehavior: Bound

import QtQuick
import kDrive.UI

Rectangle {
    id: root

    required property var controller

    radius: IKRadius.r16
    clip: true
    color: IKColors.surfaceSecondary
    border.width: 1
    border.color: IKColors.accentSecondary

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: root.border.width
        anchors.rightMargin: root.border.width
        anchors.topMargin: root.border.width
        height: Math.min(Math.max(0, parent.height - 2 * root.border.width), 180)
        radius: Math.max(0, root.radius - root.border.width)
        opacity: IKColors.darkMode ? 0.16 : 0.24
        gradient: Gradient {
            GradientStop {
                position: 0
                color: IKColors.accentSecondary
            }
            GradientStop {
                position: 1
                color: "transparent"
            }
        }
    }

    Loader {
        anchors.fill: parent
        anchors.margins: IKSpacing.s16
        sourceComponent: {
            switch (root.controller.status) {
            case HomeController.UpToDate:
                return upToDateComponent
            case HomeController.Syncing:
                return syncingComponent
            case HomeController.Paused:
                return pausedComponent
            case HomeController.Offline:
                return offlineComponent
            case HomeController.SetupRequired:
                return setupRequiredComponent
            default:
                return loadingComponent
            }
        }
    }

    Component {
        id: upToDateComponent

        HomeUpToDateState {
            controller: root.controller
            anchors.fill: parent
        }
    }

    Component {
        id: syncingComponent

        HomeSyncingState {
            controller: root.controller
            anchors.fill: parent
        }
    }

    Component {
        id: pausedComponent

        HomePausedState {
            controller: root.controller
            anchors.fill: parent
        }
    }

    Component {
        id: offlineComponent

        HomeOfflineState {
            controller: root.controller
            anchors.fill: parent
        }
    }

    Component {
        id: setupRequiredComponent

        HomeSetupRequiredState {
            controller: root.controller
            anchors.fill: parent
        }
    }

    Component {
        id: loadingComponent

        HomeLoadingState {
            controller: root.controller
            anchors.fill: parent
        }
    }
}
