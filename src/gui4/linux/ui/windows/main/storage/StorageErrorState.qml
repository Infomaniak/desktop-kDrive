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
import QtQuick.Controls
import kDrive.UI

Item {
    id: root

    required property var controller

    Column {
        anchors.centerIn: parent
        spacing: IKStorage.errorContentSpacing

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
            width: Math.min(IKStorage.errorTextMaxWidth, root.width - IKSpacing.s48)

            Text {
                color: IKColors.textPrimary
                font.pixelSize: IKStorage.errorTitleSize
                font.weight: IKFonts.emphasized
                horizontalAlignment: Text.AlignHCenter
                lineHeight: IKStorage.errorLineHeight
                lineHeightMode: Text.FixedHeight
                text: qsTrId("linuxStorageLocationUnavailableTitle")
                width: parent.width
                wrapMode: Text.WordWrap
            }
            Text {
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.bodySize
                horizontalAlignment: Text.AlignHCenter
                lineHeight: IKStorage.errorLineHeight
                lineHeightMode: Text.FixedHeight
                text: qsTrId("linuxStorageLocationUnavailableDescription")
                width: parent.width
                wrapMode: Text.WordWrap
            }
            Item {
                anchors.horizontalCenter: parent.horizontalCenter
                height: retryButton.implicitHeight
                width: retryButton.implicitWidth

                Button {
                    id: retryButton

                    anchors.fill: parent
                    flat: true
                    focusPolicy: Qt.StrongFocus
                    text: qsTrId("buttonRetry")
                    visible: !root.controller.retrying

                    contentItem: Text {
                        color: IKColors.actionPrimary
                        font.pixelSize: IKFonts.bodySize
                        horizontalAlignment: Text.AlignHCenter
                        text: retryButton.text
                        verticalAlignment: Text.AlignVCenter
                    }

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
