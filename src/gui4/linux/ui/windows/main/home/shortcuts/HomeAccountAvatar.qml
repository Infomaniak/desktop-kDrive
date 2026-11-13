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

// Avatar of the current user, which opens the account settings, with the badge of the selected synchronization, which
// opens the settings of its drive or of the advanced synchronization. The badge overflows the avatar bounds.
Item {
    id: root

    // Prefix of the user action log ids of this file (see UserActionLog).
    readonly property string actionIdPrefix: "main.home.avatar"

    required property var controller
    property color maskColor: IKColors.surfaceSecondary

    implicitWidth: IKMainWindow.homeAvatarSize
    implicitHeight: IKMainWindow.homeAvatarSize

    AbstractButton {
        id: avatarButton

        anchors.fill: parent
        padding: 0
        focusPolicy: Qt.StrongFocus
        hoverEnabled: true
        Accessible.name: root.controller.userName
        Accessible.description: qsTrId("buttonSettings")
        onClicked: {
            UserActionLog.action(root.actionIdPrefix + ".userSettings");
            root.controller.openUserSettings();
        }

        background: null

        contentItem: Item {
            IKAvatar {
                anchors.fill: parent
                source: root.controller.avatarSource
                fallbackLabel: root.controller.firstName
                maskColor: root.maskColor
            }

            Rectangle {
                anchors.fill: parent
                radius: width / 2
                visible: avatarButton.hovered || avatarButton.visualFocus
                color: IKColors.homeSettingsShortcutSurface

                IKTintedIcon {
                    anchors.centerIn: parent
                    width: IKMainWindow.homeAvatarSettingsIconSize
                    height: IKMainWindow.homeAvatarSettingsIconSize
                    source: "qrc:/assets/main/home/cog.svg"
                    color: IKColors.homeSettingsShortcutGlyph
                }
            }
        }

        HoverHandler {
            cursorShape: Qt.PointingHandCursor
        }

        IKToolTip {
            targetButton: avatarButton
            text: root.controller.userName
        }
    }

    AbstractButton {
        id: syncBadgeButton

        readonly property bool showsSettings: hovered || visualFocus

        x: IKMainWindow.homeAvatarSize - width + IKMainWindow.homeSyncBadgeOverflowX
        y: IKMainWindow.homeAvatarSize - height + IKMainWindow.homeSyncBadgeOverflowY
        width: IKMainWindow.homeSyncBadgeSize
        height: IKMainWindow.homeSyncBadgeSize
        padding: 0
        focusPolicy: Qt.StrongFocus
        hoverEnabled: true
        Accessible.name: root.controller.driveName
        Accessible.description: qsTrId("buttonOpenSyncSettings")
        onClicked: {
            UserActionLog.action(root.actionIdPrefix + ".syncSettings");
            root.controller.openSyncSettings();
        }

        background: Rectangle {
            radius: width / 2
            color: syncBadgeButton.showsSettings ? IKColors.homeSettingsShortcutSurface : IKColors.homeSyncBadgeSurface
        }

        contentItem: Item {
            IKTintedIcon {
                anchors.centerIn: parent
                width: IKMainWindow.homeSyncBadgeGlyphSize
                height: IKMainWindow.homeSyncBadgeGlyphSize
                visible: !syncBadgeButton.showsSettings
                source: root.controller.advancedSync ? "qrc:/assets/main/folder.svg"
                                                     : "qrc:/assets/onboarding/drive-icon-glyph.svg"
                color: root.controller.driveColor
            }

            IKTintedIcon {
                anchors.centerIn: parent
                width: IKMainWindow.homeSyncBadgeSettingsIconSize
                height: IKMainWindow.homeSyncBadgeSettingsIconSize
                visible: syncBadgeButton.showsSettings
                source: "qrc:/assets/main/home/cog.svg"
                color: IKColors.homeSettingsShortcutGlyph
            }
        }

        HoverHandler {
            cursorShape: Qt.PointingHandCursor
        }

        IKToolTip {
            targetButton: syncBadgeButton
            text: root.controller.driveName
        }
    }
}
