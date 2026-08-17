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
    required property var controller

    readonly property int currentTab: root.appRouter.currentMainTabIndex
    readonly property int tabActivities: AppRouter.Activities
    readonly property int tabStorage: AppRouter.Storage
    readonly property int tabBlockingError: AppRouter.BlockingError

    color: "transparent"

    Text {
        id: titleLabel

        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(implicitWidth, Math.max(0, actionsRow.x - x - IKSpacing.s16))
        text: {
            switch (root.currentTab) {
            case root.tabActivities:
                return qsTrId("tabTitleActivities")
            case root.tabStorage:
                return qsTrId("tabTitleStorage")
            case root.tabBlockingError:
                return qsTrId("errorPageTitle")
            default:
                return qsTrId("tabTitleHome")
            }
        }
        color: IKColors.textPrimary
        font.pixelSize: IKFonts.headlineSize
        font.weight: IKFonts.emphasized
        elide: Text.ElideRight
    }

    Row {
        id: actionsRow

        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: IKSpacing.s8

        HeaderIconButton {
            iconSource: "qrc:/assets/main/home/headphones.svg"
            text: qsTrId("infomaniakSupport")
            onClicked: root.controller.openSupport()
        }

        Rectangle {
            width: IKMainWindow.toolbarActionGroupWidth
            height: IKMainWindow.toolbarActionGroupHeight
            radius: height / 2
            color: IKColors.toolbarControlSurface
            border.width: 1
            border.color: IKColors.surfaceTertiary

            Row {
                anchors.centerIn: parent
                spacing: IKMainWindow.toolbarActionGroupSpacing

                GroupedIconButton {
                    enabled: root.controller.syncControlState === HomeController.Pause
                             || root.controller.syncControlState === HomeController.Resume
                    iconSource: root.controller.syncControlState === HomeController.Resume
                                ? "qrc:/assets/main/home/play.svg" : "qrc:/assets/main/home/pause.svg"
                    text: root.controller.syncControlState === HomeController.Resume
                          ? qsTrId("buttonRestartSync") : qsTrId("buttonPause")
                    onClicked: root.controller.toggleSync()
                }

                FutureGroupedIconButton {
                    iconSource: "qrc:/assets/main/home/cog.svg"
                    accessibleName: qsTrId("buttonSettings")
                }
            }
        }

        HeaderIconButton {
            iconSource: "qrc:/assets/main/home/search.svg"
            iconSize: IKMainWindow.toolbarSearchIconSize
            text: qsTrId("buttonSearch")
            tooltipText: qsTrId("comingSoon")
            Accessible.description: qsTrId("comingSoon")
        }
    }

    component HeaderIconButton: ToolButton {
        id: buttonRoot

        required property url iconSource
        property real iconSize: IKIconSizes.medium
        property string tooltipText: text

        width: IKMainWindow.toolbarIconButtonSize
        height: IKMainWindow.toolbarIconButtonSize
        focusPolicy: Qt.StrongFocus
        hoverEnabled: true
        display: AbstractButton.IconOnly

        contentItem: Item {
            IKTintedIcon {
                anchors.centerIn: parent
                width: buttonRoot.iconSize
                height: buttonRoot.iconSize
                source: buttonRoot.iconSource
                color: buttonRoot.enabled ? IKColors.textSecondary : IKColors.actionDisabled
            }
        }

        background: Rectangle {
            radius: width / 2
            color: buttonRoot.hovered || buttonRoot.down ? IKColors.toolbarControlHover : IKColors.toolbarControlSurface
            border.width: buttonRoot.visualFocus ? 2 : 1
            border.color: buttonRoot.visualFocus ? IKColors.accentPrimary : IKColors.surfaceTertiary
        }

        IKToolTip {
            visible: buttonRoot.hovered || buttonRoot.activeFocus
            text: buttonRoot.tooltipText
        }
    }

    component GroupedIconButton: ToolButton {
        id: groupedButton

        required property url iconSource

        width: IKMainWindow.toolbarActionGroupButtonSize
        height: IKMainWindow.toolbarActionGroupButtonSize
        focusPolicy: Qt.StrongFocus
        hoverEnabled: true
        display: AbstractButton.IconOnly

        contentItem: IKTintedIcon {
            width: IKIconSizes.medium
            height: IKIconSizes.medium
            source: groupedButton.iconSource
            color: groupedButton.enabled ? IKColors.textSecondary : IKColors.actionDisabled
        }

        background: Rectangle {
            radius: width / 2
            color: groupedButton.hovered || groupedButton.down ? IKColors.toolbarControlHover : "transparent"
            border.width: groupedButton.visualFocus ? 2 : 0
            border.color: IKColors.accentPrimary
        }

        IKToolTip {
            visible: groupedButton.hovered || groupedButton.activeFocus
            text: groupedButton.text
        }
    }

    component FutureGroupedIconButton: Item {
        id: futureGroupedButton

        required property url iconSource
        required property string accessibleName

        width: IKMainWindow.toolbarActionGroupButtonSize
        height: IKMainWindow.toolbarActionGroupButtonSize

        ToolButton {
            id: settingsButton

            anchors.fill: parent
            display: AbstractButton.IconOnly
            focusPolicy: Qt.StrongFocus
            hoverEnabled: true
            Accessible.name: futureGroupedButton.accessibleName
            Accessible.description: qsTrId("comingSoon")

            contentItem: IKTintedIcon {
                width: IKIconSizes.medium
                height: IKIconSizes.medium
                source: futureGroupedButton.iconSource
                color: IKColors.textSecondary
            }

            background: Rectangle {
                radius: width / 2
                color: settingsButton.hovered || settingsButton.down ? IKColors.toolbarControlHover : "transparent"
                border.width: settingsButton.visualFocus ? 2 : 0
                border.color: IKColors.accentPrimary
            }
        }

        IKToolTip {
            visible: settingsButton.hovered || settingsButton.activeFocus
            text: qsTrId("comingSoon")
        }
    }

}
