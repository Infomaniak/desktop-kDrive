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
    property int currentTab: 0

    readonly property int tabActivities: 1
    readonly property int tabStorage: 2
    readonly property int tabBlockingError: 3

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
            width: 132
            height: 36
            radius: height / 2
            color: IKColors.surfaceSecondary
            border.width: 1
            border.color: IKColors.surfaceTertiary

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onEntered: parent.color = IKColors.surfaceTertiary
                onExited: parent.color = IKColors.surfaceSecondary
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

        width: 32
        height: 32
        radius: width / 2
        color: IKColors.surfaceSecondary
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
            onEntered: buttonRoot.color = IKColors.surfaceTertiary
            onExited: buttonRoot.color = IKColors.surfaceSecondary
            onClicked: buttonRoot.triggered()
        }
    }
}
