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
            label: "?"
            onTriggered: root.appRouter.openSupport()
        }

        HeaderIconButton {
            label: "||"
            onTriggered: root.appRouter.requestPauseCurrentSync()
        }

        Rectangle {
            id: searchButton

            property bool hovered: false

            width: 132
            height: 36
            radius: height / 2
            color: hovered ? IKColors.surfaceTertiary : IKColors.surfaceSecondary
            border.width: 1
            border.color: IKColors.surfaceTertiary

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onEntered: searchButton.hovered = true
                onExited: searchButton.hovered = false
                onClicked: root.appRouter.requestSearch()
            }

            Row {
                anchors.left: parent.left
                anchors.leftMargin: IKSpacing.s12
                anchors.right: parent.right
                anchors.rightMargin: IKSpacing.s12
                anchors.verticalCenter: parent.verticalCenter
                spacing: IKSpacing.s8

                Text {
                    text: qsTrId("buttonSearch")
                    color: IKColors.textTertiary
                    font.pixelSize: IKFonts.bodySize
                    elide: Text.ElideRight
                }
            }
        }
    }

    component HeaderIconButton: Rectangle {
        id: buttonRoot

        signal triggered()

        property string label: ""
        property bool hovered: false

        width: 32
        height: 32
        radius: width / 2
        color: hovered ? IKColors.surfaceTertiary : IKColors.surfaceSecondary
        border.width: 1
        border.color: IKColors.surfaceTertiary

        Text {
            anchors.centerIn: parent
            text: buttonRoot.label
            color: IKColors.textSecondary
            font.pixelSize: IKFonts.bodySize
            font.weight: IKFonts.emphasized
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onEntered: buttonRoot.hovered = true
            onExited: buttonRoot.hovered = false
            onClicked: buttonRoot.triggered()
        }
    }
}
