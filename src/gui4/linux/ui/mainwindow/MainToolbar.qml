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
            glyph: "?"
            text: qsTrId("infomaniakSupport")
            onClicked: root.appRouter.openSupport()
        }

        HeaderIconButton {
            glyph: "||"
            text: qsTrId("buttonPause")
            onClicked: root.appRouter.requestPauseCurrentSync()
        }

        Button {
            id: searchButton

            width: 132
            height: 36
            leftPadding: IKSpacing.s12
            rightPadding: IKSpacing.s12
            focusPolicy: Qt.StrongFocus
            hoverEnabled: true
            text: qsTrId("buttonSearch")
            onClicked: root.appRouter.requestSearch()

            contentItem: Text {
                text: searchButton.text
                color: IKColors.textTertiary
                font.pixelSize: IKFonts.bodySize
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            background: Rectangle {
                radius: height / 2
                color: searchButton.hovered || searchButton.down ? IKColors.surfaceTertiary : IKColors.surfaceSecondary
                border.width: searchButton.visualFocus ? 2 : 1
                border.color: searchButton.visualFocus ? IKColors.accentPrimary : IKColors.surfaceTertiary
            }
        }
    }

    component HeaderIconButton: ToolButton {
        id: buttonRoot

        required property string glyph

        width: 32
        height: 32
        focusPolicy: Qt.StrongFocus
        hoverEnabled: true
        display: AbstractButton.IconOnly

        contentItem: Text {
            text: buttonRoot.glyph
            color: IKColors.textSecondary
            font.pixelSize: IKFonts.bodySize
            font.weight: IKFonts.emphasized
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: width / 2
            color: buttonRoot.hovered || buttonRoot.down ? IKColors.surfaceTertiary : IKColors.surfaceSecondary
            border.width: buttonRoot.visualFocus ? 2 : 1
            border.color: buttonRoot.visualFocus ? IKColors.accentPrimary : IKColors.surfaceTertiary
        }

        ToolTip.visible: buttonRoot.hovered || buttonRoot.activeFocus
        ToolTip.text: buttonRoot.text
        ToolTip.delay: 500
    }
}
