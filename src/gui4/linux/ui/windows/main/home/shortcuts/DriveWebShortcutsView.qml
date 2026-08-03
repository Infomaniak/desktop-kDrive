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

Rectangle {
    id: root

    required property var controller

    readonly property real shortcutsImplicitWidth: Math.max(favoritesButton.implicitWidth,
                                                            sharedButton.implicitWidth,
                                                            onlineDriveButton.implicitWidth,
                                                            trashButton.implicitWidth)
    readonly property real driveNameImplicitWidth: Math.min(driveNameLabel.implicitWidth,
                                                            IKMainWindow.homeDriveNameMaxWidth)

    implicitWidth: 2 * IKSpacing.s16 + Math.max(shortcutsImplicitWidth, driveNameImplicitWidth)
    radius: IKRadius.r16
    color: IKColors.surfaceSecondary
    border.width: 1
    border.color: IKColors.surfaceTertiary

    Column {
        anchors.fill: parent
        anchors.topMargin: IKSpacing.s24
        anchors.bottomMargin: IKSpacing.s16
        anchors.leftMargin: IKSpacing.s16
        anchors.rightMargin: IKSpacing.s16
        spacing: IKSpacing.s16

        Column {
            width: parent.width
            spacing: IKSpacing.s16

            IKAvatar {
                width: IKMainWindow.homeAvatarSize
                height: IKMainWindow.homeAvatarSize
                anchors.horizontalCenter: parent.horizontalCenter
                source: root.controller.avatarSource
                fallbackLabel: root.controller.firstName
                maskColor: root.color
            }

            Text {
                id: driveNameLabel

                width: Math.min(implicitWidth, parent.width, IKMainWindow.homeDriveNameMaxWidth)
                height: Math.min(implicitHeight, IKMainWindow.homeDriveNameMaxHeight)
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.controller.driveName
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.bodySize
                font.weight: IKFonts.emphasized
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignTop
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight

                MouseArea {
                    id: driveNamePointer

                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }

                ToolTip {
                    id: driveNameTooltip

                    visible: driveNamePointer.containsMouse && driveNameLabel.truncated
                    delay: IKMainWindow.syncSelectorTooltipDelay
                    timeout: -1
                    text: root.controller.driveName
                    padding: IKMainWindow.syncSelectorTooltipPadding

                    contentItem: Text {
                        width: Math.min(implicitWidth, IKMainWindow.syncSelectorTooltipMaxWidth)
                        text: driveNameTooltip.text
                        color: IKColors.tooltipText
                        font.pixelSize: IKFonts.bodySize
                        wrapMode: Text.WordWrap
                    }

                    background: Rectangle {
                        radius: IKMainWindow.syncSelectorTooltipRadius
                        color: IKColors.tooltipSurface
                    }
                }
            }
        }

        Column {
            width: parent.width
            spacing: IKSpacing.s8

            HomeWebShortcutButton {
                id: favoritesButton

                width: parent.width
                text: qsTrId("folderFavorites")
                glyphSource: "qrc:/assets/main/home/star.svg"
                onClicked: root.controller.openDriveDestination(HomeController.Favorites)
            }

            HomeWebShortcutButton {
                id: sharedButton

                width: parent.width
                text: qsTrId("folderShares")
                glyphSource: "qrc:/assets/main/home/folder-share.svg"
                onClicked: root.controller.openDriveDestination(HomeController.Shared)
            }

            HomeWebShortcutButton {
                id: onlineDriveButton

                width: parent.width
                text: qsTrId("buttonKDriveOnline")
                glyphSource: "qrc:/assets/main/home/kdrive-folders-stacked.svg"
                onClicked: root.controller.openDriveDestination(HomeController.OnlineDrive)
            }
        }

        Item {
            width: 1
            height: Math.max(0, parent.height - y - trashButton.height - parent.spacing)
        }

        HomeWebShortcutButton {
            id: trashButton

            width: parent.width
            text: qsTrId("folderTrash")
            glyphSource: "qrc:/assets/main/home/trash.svg"
            onClicked: root.controller.openDriveDestination(HomeController.Trash)
        }
    }
}
