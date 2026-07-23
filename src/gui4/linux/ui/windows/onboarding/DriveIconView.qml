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

    property color iconColor: IKColors.driveDefaultColor

    implicitWidth: IKOnboarding.driveSelectionDriveIconSize
    implicitHeight: IKOnboarding.driveSelectionDriveIconSize

    Rectangle {
        anchors.fill: parent
        radius: IKOnboarding.driveSelectionDriveIconRadius
        color: root.iconColor
        border.width: IKOnboarding.driveSelectionDriveIconBorderWidth
        border.color: IKColors.onboardingDriveIconBorder
    }

    Rectangle {
        anchors.fill: parent
        radius: IKOnboarding.driveSelectionDriveIconRadius
        gradient: Gradient {
            orientation: Gradient.Vertical

            GradientStop {
                position: 0
                color: IKColors.onboardingDriveIconHighlightStart
            }

            GradientStop {
                position: 1
                color: IKColors.onboardingDriveIconHighlightEnd
            }
        }
    }

    Image {
        anchors.fill: parent
        anchors.margins: IKOnboarding.driveSelectionDriveIconGlyphInset
        source: "qrc:/assets/onboarding/drive-icon-glyph.svg"
        fillMode: Image.Stretch
        smooth: true
    }

    Rectangle {
        anchors.fill: parent
        radius: IKOnboarding.driveSelectionDriveIconRadius
        color: "transparent"
        border.width: IKOnboarding.driveSelectionDriveIconBorderWidth
        border.color: IKColors.onboardingDriveIconInnerBorder
    }
}
