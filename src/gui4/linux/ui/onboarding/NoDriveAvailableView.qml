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
import QtQuick.Controls
import kDrive.UI

Column {
    id: root

    width: parent ? parent.width : IKOnboarding.driveSelectionContentMaxWidth
    spacing: IKSpacing.s24

    Column {
        width: parent.width
        spacing: IKSpacing.s8

        Text {
            width: parent.width
            text: qsTr("You don’t have a kDrive yet.")
            color: IKColors.textPrimary
            font.pixelSize: IKFonts.title3Size
            font.weight: IKFonts.emphasized
            lineHeightMode: Text.FixedHeight
            lineHeight: IKOnboarding.driveSelectionEmptyTitleLineHeight
            wrapMode: Text.WordWrap
        }

        Text {
            width: parent.width
            text: qsTr("Get started for free with my kSuite,\nor choose a package tailored to your needs.")
            color: IKColors.textSecondary
            font.pixelSize: IKFonts.bodySize
            lineHeightMode: Text.FixedHeight
            lineHeight: IKOnboarding.driveSelectionDriveNameLineHeight
            wrapMode: Text.WordWrap
        }
    }

    Row {
        spacing: IKSpacing.s8

        Button {
            id: showOffersButton

            height: IKOnboarding.driveSelectionButtonHeight
            text: qsTr("Show offers")
            onClicked: availableDrivesModel.openDriveOffers()

            contentItem: Text {
                text: showOffersButton.text
                color: IKColors.actionPrimary
                font.pixelSize: IKFonts.bodySize
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            background: Rectangle {
                implicitWidth: IKOnboarding.driveSelectionSecondaryButtonMinWidth
                implicitHeight: IKOnboarding.driveSelectionButtonHeight
                radius: IKOnboarding.buttonCornerRadius
                color: "transparent"
            }

            padding: 0
            leftPadding: IKSpacing.s16
            rightPadding: IKSpacing.s16
            topPadding: 0
            bottomPadding: 0
        }

        Button {
            id: startForFreeButton

            height: IKOnboarding.driveSelectionButtonHeight
            text: qsTr("Get started for free")
            onClicked: availableDrivesModel.startForFree()

            contentItem: Text {
                text: startForFreeButton.text
                color: IKColors.actionOnPrimary
                font.pixelSize: IKFonts.bodySize
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            background: Rectangle {
                implicitWidth: IKOnboarding.driveSelectionButtonMinWidth
                implicitHeight: IKOnboarding.driveSelectionButtonHeight
                radius: IKOnboarding.buttonCornerRadius
                color: IKColors.actionPrimary
            }

            padding: 0
            leftPadding: IKSpacing.s16
            rightPadding: IKSpacing.s16
            topPadding: 0
            bottomPadding: 0
        }
    }
}
