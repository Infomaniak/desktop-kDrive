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

Rectangle {
    id: root

    required property var userDbId
    required property string name
    required property string email
    required property string avatarSource
    required property var drivesModel
    required property bool availableDrivesLoading
    required property bool availableDrivesFailed
    property bool expanded: true
    readonly property color headerSurfaceColor: headerButton.down || headerButton.hovered
                                                ? IKColors.surfaceTertiary
                                                : IKColors.settingsCardSurface

    signal retryRequested

    implicitHeight: contentColumn.implicitHeight + 2 * IKSettings.userCardPadding
    radius: IKRadius.r12
    color: IKColors.settingsCardSurface

    Column {
        id: contentColumn

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: IKSettings.userCardPadding

        Button {
            id: headerButton

            width: parent.width
            height: IKSettings.userHeaderHeight
            padding: 0
            focusPolicy: Qt.StrongFocus
            hoverEnabled: true
            Accessible.name: root.name
            Accessible.description: root.email
            Accessible.checkable: true
            Accessible.checked: root.expanded
            Accessible.onToggleAction: headerButton.clicked()
            onClicked: root.expanded = !root.expanded

            background: Rectangle {
                radius: IKRadius.r8
                color: root.headerSurfaceColor
                border.width: headerButton.visualFocus ? 2 : 0
                border.color: IKColors.accentPrimary
            }

            contentItem: Item {
                IKAvatar {
                    id: avatar

                    anchors.left: parent.left
                    anchors.leftMargin: IKSpacing.s8
                    anchors.verticalCenter: parent.verticalCenter
                    width: IKSettings.userAvatarSize
                    height: width
                    source: root.avatarSource
                    fallbackLabel: root.name
                    maskColor: root.headerSurfaceColor
                }

                Column {
                    anchors.left: avatar.right
                    anchors.leftMargin: IKSpacing.s12
                    anchors.right: disclosure.left
                    anchors.rightMargin: IKSpacing.s12
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: IKSpacing.s2

                    Text {
                        width: parent.width
                        text: root.name
                        color: IKColors.textPrimary
                        font.pixelSize: IKFonts.bodySize
                        font.weight: IKFonts.emphasized
                        elide: Text.ElideRight
                    }

                    Text {
                        width: parent.width
                        text: root.email
                        color: IKColors.textSecondary
                        font.pixelSize: IKFonts.subheadlineSize
                        elide: Text.ElideRight
                    }
                }

                IKTintedIcon {
                    id: disclosure

                    anchors.right: parent.right
                    anchors.rightMargin: IKSpacing.s8
                    anchors.verticalCenter: parent.verticalCenter
                    width: IKSettings.navigationIconSize
                    height: width
                    source: "qrc:/assets/main/chevron-down.svg"
                    color: IKColors.textSecondary
                    rotation: root.expanded ? 0 : -90
                }
            }
        }

        Column {
            width: parent.width
            visible: root.expanded

            Repeater {
                model: root.drivesModel

                delegate: UserDriveRow {
                    width: contentColumn.width
                }
            }

            Item {
                width: parent.width
                height: visible ? IKSettings.userStatusRowHeight : 0
                visible: root.availableDrivesLoading

                IKLoadingSpinner {
                    anchors.centerIn: parent
                    width: IKSettings.userStatusIconSize
                    height: width
                    strokeWidth: 2
                    Accessible.name: qsTrId("onboardingLoginHintLoading")
                }
            }

            Item {
                width: parent.width
                height: visible ? IKSettings.availableDriveFailureRowHeight : 0
                visible: root.availableDrivesFailed && !root.availableDrivesLoading

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 1
                    color: IKColors.settingsDivider
                }

                Column {
                    anchors.left: parent.left
                    anchors.leftMargin: IKSettings.userDriveLeadingIndent
                    anchors.right: retryButton.left
                    anchors.rightMargin: IKSpacing.s16
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: IKSpacing.s2

                    Text {
                        width: parent.width
                        text: qsTrId("onboardingLoginErrorTitle")
                        color: IKColors.textPrimary
                        font.pixelSize: IKFonts.bodySize
                        font.weight: IKFonts.medium
                        elide: Text.ElideRight
                        Accessible.role: Accessible.AlertMessage
                    }

                    Text {
                        width: parent.width
                        text: qsTrId("onboardingLoginErrorDescription")
                        color: IKColors.textSecondary
                        font.pixelSize: IKFonts.subheadlineSize
                        elide: Text.ElideRight
                    }
                }

                IKModalButton {
                    id: retryButton

                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    implicitHeight: IKSettings.connectAccountButtonHeight
                    role: IKModalButton.Tonal
                    text: qsTrId("buttonRetry")
                    onClicked: root.retryRequested()
                }
            }

            Item {
                width: parent.width
                height: IKSettings.userFooterHeight

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 1
                    color: IKColors.settingsDivider
                }

                IKModalButton {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    implicitHeight: IKSettings.connectAccountButtonHeight
                    role: IKModalButton.DestructiveSecondary
                    text: qsTrId("buttonDisconnectAccount")
                    actionEnabled: false
                }
            }
        }
    }
}
