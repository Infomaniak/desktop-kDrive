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

Rectangle {
    id: root

    required property var appRouter

    readonly property int currentTab: root.appRouter.currentMainTabIndex
    readonly property int tabHome: AppRouter.Home
    readonly property int tabActivities: AppRouter.Activities
    readonly property int tabStorage: AppRouter.Storage

    function navigateTo(tabIndex: int) {
        root.appRouter.navigateToMainTab(tabIndex)
    }

    color: IKColors.surfaceSecondary

    Column {
        id: sidebarColumn

        anchors.fill: parent
        anchors.margins: IKSpacing.s16
        spacing: IKSpacing.s16

        Row {
            width: parent.width
            height: 44
            spacing: IKSpacing.s12

            Rectangle {
                width: 36
                height: 36
                radius: width / 2
                color: IKColors.driveDefaultColor
                anchors.verticalCenter: parent.verticalCenter
            }

            Column {
                width: parent.width - 48
                anchors.verticalCenter: parent.verticalCenter
                spacing: IKSpacing.s2

                Text {
                    width: parent.width
                    text: "kDrive"
                    color: IKColors.textPrimary
                    font.pixelSize: IKFonts.bodySize
                    font.weight: IKFonts.emphasized
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    text: qsTrId("labelSynchronisation")
                    color: IKColors.textSecondary
                    font.pixelSize: IKFonts.subheadlineSize
                    elide: Text.ElideRight
                }
            }
        }

        Column {
            width: parent.width
            spacing: IKSpacing.s4

            Rectangle {
                id: homeItem

                property bool hovered: false

                width: parent.width
                height: 36
                radius: IKRadius.r8
                color: root.currentTab === root.tabHome ? IKColors.surfaceTertiary
                                                        : hovered ? IKColors.surfacePrimary
                                                                  : "transparent"

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: IKSpacing.s12
                    anchors.right: parent.right
                    anchors.rightMargin: IKSpacing.s12
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTrId("tabTitleHome")
                    color: root.currentTab === root.tabHome ? IKColors.textPrimary : IKColors.textSecondary
                    font.pixelSize: IKFonts.bodySize
                    font.weight: root.currentTab === root.tabHome ? IKFonts.emphasized : Font.Normal
                    elide: Text.ElideRight
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton
                    cursorShape: Qt.PointingHandCursor
                    onEntered: homeItem.hovered = true
                    onExited: homeItem.hovered = false
                    onClicked: root.navigateTo(root.tabHome)
                }
            }

            Rectangle {
                id: activitiesItem

                property bool hovered: false

                width: parent.width
                height: 36
                radius: IKRadius.r8
                color: root.currentTab === root.tabActivities ? IKColors.surfaceTertiary
                                                              : hovered ? IKColors.surfacePrimary
                                                                        : "transparent"

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: IKSpacing.s12
                    anchors.right: parent.right
                    anchors.rightMargin: IKSpacing.s12
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTrId("tabTitleActivities")
                    color: root.currentTab === root.tabActivities ? IKColors.textPrimary : IKColors.textSecondary
                    font.pixelSize: IKFonts.bodySize
                    font.weight: root.currentTab === root.tabActivities ? IKFonts.emphasized : Font.Normal
                    elide: Text.ElideRight
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton
                    cursorShape: Qt.PointingHandCursor
                    onEntered: activitiesItem.hovered = true
                    onExited: activitiesItem.hovered = false
                    onClicked: root.navigateTo(root.tabActivities)
                }
            }

            Rectangle {
                id: storageItem

                property bool hovered: false

                width: parent.width
                height: 36
                radius: IKRadius.r8
                color: root.currentTab === root.tabStorage ? IKColors.surfaceTertiary
                                                           : hovered ? IKColors.surfacePrimary
                                                                     : "transparent"

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: IKSpacing.s12
                    anchors.right: parent.right
                    anchors.rightMargin: IKSpacing.s12
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTrId("tabTitleStorage")
                    color: root.currentTab === root.tabStorage ? IKColors.textPrimary : IKColors.textSecondary
                    font.pixelSize: IKFonts.bodySize
                    font.weight: root.currentTab === root.tabStorage ? IKFonts.emphasized : Font.Normal
                    elide: Text.ElideRight
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton
                    cursorShape: Qt.PointingHandCursor
                    onEntered: storageItem.hovered = true
                    onExited: storageItem.hovered = false
                    onClicked: root.navigateTo(root.tabStorage)
                }
            }

            Rectangle {
                width: parent.width
                height: 36
                radius: IKRadius.r8
                color: "transparent"
                opacity: 0.45

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: IKSpacing.s12
                    anchors.right: parent.right
                    anchors.rightMargin: IKSpacing.s12
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTrId("buttonOpenFolder")
                    color: IKColors.textSecondary
                    font.pixelSize: IKFonts.bodySize
                    elide: Text.ElideRight
                }
            }
        }
    }
}
