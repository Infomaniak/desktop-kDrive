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

Item {
    id: root

    Rectangle {
        anchors.fill: parent
        color: IKColors.onboardingSurfacePrimary
    }

    Row {
        anchors.fill: parent
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        spacing: 0

        LoginView {
            width: parent.width - illustrationPanel.width
            height: parent.height
        }

        Rectangle {
            id: illustrationPanel

            width: Math.min(330, Math.max(260, root.width * 0.37))
            height: parent.height
            color: IKColors.onboardingSurfaceSecondary
            radius: IKRadius.r16

            OnboardingAnimationsView {
                anchors.centerIn: parent
                width: Math.min(260, parent.width * 0.8)
                height: width
            }
        }
    }
}
