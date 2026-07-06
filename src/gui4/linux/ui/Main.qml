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

pragma ComponentBehavior: Bound

import QtQuick
import kDrive.UI
import "onboarding"

Window {
    id: mainWindow

    required property var onboardingSessionManager
    required property var systemTrayController

    visible: false
    width: 900
    height: 600
    minimumWidth: 720
    minimumHeight: 520
    title: onboardingLoader.session ? onboardingLoader.session.flowController.title : qsTr("kDrive")
    color: IKColors.onboardingSurfacePrimary

    onClosing: close => {
        if (mainWindow.systemTrayController.trayModeActive) {
            close.accepted = false;
            mainWindow.systemTrayController.hideMainWindow();
        } else {
            close.accepted = true;
            Qt.quit();
        }
    }

    Connections {
        target: mainWindow.onboardingSessionManager

        function onActiveSessionChanged() {
            if (mainWindow.onboardingSessionManager.activeSession) {
                onboardingLoader.session = mainWindow.onboardingSessionManager.activeSession;
                onboardingLoader.active = true;
            } else {
                onboardingLoader.active = false;
                onboardingLoader.session = null;
            }
        }
    }

    Loader {
        id: onboardingLoader

        anchors.fill: parent
        active: false
        sourceComponent: onboardingComponent
        property var session: null

        Component.onCompleted: {
            if (mainWindow.onboardingSessionManager.activeSession) {
                session = mainWindow.onboardingSessionManager.activeSession;
                active = true;
            }
        }
    }

    Component {
        id: onboardingComponent

        OnboardingWindow {
            session: onboardingLoader.session
        }
    }
}
