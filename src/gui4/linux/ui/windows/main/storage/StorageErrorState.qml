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

// Replaces the disk usage card when the disk of the synchronization cannot be read.
Item {
    id: root

    required property var controller

    implicitHeight: IKStorage.rowHeight + errorContent.implicitHeight + IKStorage.unavailableCardBottomPadding

    Text {
        x: IKStorage.cardHorizontalPadding
        y: IKStorage.headerTopPadding
        width: parent.width - 2 * IKStorage.cardHorizontalPadding
        color: IKColors.textPrimary
        elide: Text.ElideRight
        font.pixelSize: IKFonts.bodySize
        font.weight: IKFonts.medium
        text: qsTrId("linuxStorageLocalTitle")
        textFormat: Text.PlainText
    }

    Column {
        id: errorContent

        anchors.horizontalCenter: parent.horizontalCenter
        y: IKStorage.rowHeight
        width: Math.max(0, parent.width - IKSpacing.s48)

        Item {
            anchors.horizontalCenter: parent.horizontalCenter
            height: IKStorage.illustrationSize
            width: IKStorage.illustrationSize

            Image {
                anchors.centerIn: parent
                fillMode: Image.PreserveAspectFit
                height: IKStorage.illustrationGraphicHeight
                smooth: true
                source: "qrc:/assets/main/storage/unavailable.svg"
                sourceSize.height: height
                sourceSize.width: width
                width: IKStorage.illustrationGraphicWidth
            }
        }
        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: IKStorage.errorTextSpacing
            width: parent.width

            Text {
                color: IKColors.textPrimary
                font.pixelSize: IKStorage.errorTitleSize
                font.weight: IKFonts.emphasized
                horizontalAlignment: Text.AlignHCenter
                lineHeight: IKStorage.errorLineHeight
                lineHeightMode: Text.FixedHeight
                text: qsTrId("linuxStorageMissingDiskTitle")
                width: parent.width
                wrapMode: Text.WordWrap
            }
            // Narrower than the title, so the description breaks into two balanced lines.
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.bodySize
                horizontalAlignment: Text.AlignHCenter
                lineHeight: IKStorage.errorLineHeight
                lineHeightMode: Text.FixedHeight
                text: qsTrId("linuxStorageMissingDiskDescription")
                width: Math.min(IKStorage.errorDescriptionMaxWidth, parent.width)
                wrapMode: Text.WordWrap
            }
            Item {
                anchors.horizontalCenter: parent.horizontalCenter
                height: retryButton.implicitHeight
                width: retryButton.implicitWidth

                IKLinkButton {
                    id: retryButton

                    anchors.fill: parent
                    actionId: "main.storage.retry"
                    text: qsTrId("buttonRetry")
                    visible: !root.controller.retrying

                    onClicked: root.controller.retry()
                }
                IKLoadingSpinner {
                    Accessible.name: qsTrId("storageLoadingHint")
                    anchors.centerIn: parent
                    height: IKStorage.loadingSpinnerSize
                    visible: root.controller.retrying
                    width: IKStorage.loadingSpinnerSize
                }
            }
        }
    }
}
