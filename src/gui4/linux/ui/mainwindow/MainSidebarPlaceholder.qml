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

            NavigationButton {
                width: parent.width
                text: qsTrId("tabTitleHome")
                selected: root.currentTab === root.tabHome
                onClicked: root.navigateTo(root.tabHome)
            }

            NavigationButton {
                width: parent.width
                text: qsTrId("tabTitleActivities")
                selected: root.currentTab === root.tabActivities
                onClicked: root.navigateTo(root.tabActivities)
            }

            NavigationButton {
                width: parent.width
                text: qsTrId("tabTitleStorage")
                selected: root.currentTab === root.tabStorage
                onClicked: root.navigateTo(root.tabStorage)
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

    component NavigationButton: Button {
        id: navigationButton

        required property bool selected

        height: 36
        leftPadding: IKSpacing.s12
        rightPadding: IKSpacing.s12
        focusPolicy: Qt.StrongFocus
        hoverEnabled: true

        contentItem: Text {
            text: navigationButton.text
            color: navigationButton.selected ? IKColors.textPrimary : IKColors.textSecondary
            font.pixelSize: IKFonts.bodySize
            font.weight: navigationButton.selected ? IKFonts.emphasized : Font.Normal
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: IKRadius.r8
            color: navigationButton.selected || navigationButton.down ? IKColors.surfaceTertiary
                                                                       : navigationButton.hovered ? IKColors.surfacePrimary
                                                                                                  : "transparent"
            border.width: navigationButton.visualFocus ? 2 : 0
            border.color: IKColors.accentPrimary
        }
    }
}
