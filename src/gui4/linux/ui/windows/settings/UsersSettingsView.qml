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
import QtQuick.Controls
import kDrive.UI

Flickable {
    id: root

    required property var controller
    property string navigationTitle: qsTrId("sidebarItemAccounts")
    readonly property bool hasUsers: controller.usersModel.count > 0

    contentWidth: width
    contentHeight: contentColumn.implicitHeight + 2 * IKSettings.pageMargin
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar {}

    Column {
        id: contentColumn

        x: IKSettings.pageMargin
        y: IKSettings.pageMargin
        width: root.width - 2 * IKSettings.pageMargin
        spacing: IKSettings.groupSpacing

        SettingsGroup {
            width: parent.width
            visible: !root.hasUsers

            Item {
                width: parent.width
                height: IKSettings.emptyUserCardHeight

                Rectangle {
                    id: emptyUserAvatar

                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: IKSettings.emptyUserAvatarSize
                    height: width
                    radius: width / 2
                    color: IKColors.settingsEmptyUserAvatarSurface
                    border.width: 1
                    border.color: IKColors.settingsEmptyUserAvatarBorder

                    Image {
                        anchors.centerIn: parent
                        width: IKSettings.emptyUserAvatarIconSize
                        height: width
                        source: "qrc:/assets/settings/single-user.svg"
                        sourceSize.width: width
                        sourceSize.height: height
                    }
                }

                Text {
                    anchors.left: emptyUserAvatar.right
                    anchors.leftMargin: IKSpacing.s8
                    anchors.right: connectEmptyButton.left
                    anchors.rightMargin: IKSpacing.s12
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTrId("noAccountConnected")
                    color: IKColors.textPrimary
                    font.pixelSize: IKFonts.bodySize
                    wrapMode: Text.WordWrap
                }

                IKModalButton {
                    id: connectEmptyButton

                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    implicitHeight: IKSettings.connectAccountButtonHeight
                    role: IKModalButton.Tonal
                    text: qsTrId("buttonConnectAccount")
                    actionEnabled: false
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: IKColors.settingsDivider
                }
            }
        }

        Repeater {
            model: root.controller.usersModel

            delegate: UserCard {
                width: contentColumn.width
                onRetryRequested: root.controller.retryAvailableDrives(userDbId)
            }
        }

        SettingsGroup {
            width: parent.width
            visible: root.hasUsers

            Item {
                width: parent.width
                height: IKSettings.connectUserCardHeight

                IKModalButton {
                    id: connectButton

                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    implicitHeight: IKSettings.connectAccountButtonHeight
                    role: IKModalButton.Tonal
                    text: qsTrId("buttonConnectAccount")
                    actionEnabled: false
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: IKColors.settingsDivider
                }
            }
        }
    }
}
