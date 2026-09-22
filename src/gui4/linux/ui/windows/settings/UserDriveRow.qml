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
import kDrive.UI

Item {
    id: root

    required property string name
    required property string accountName
    required property color color
    required property bool isSynchronized
    required property var accountId
    required property var driveId
    property bool actionBusy: false
    readonly property bool hasDistinctAccountName: accountName.length > 0 && accountName !== name

    signal activateRequested(Item trigger, var accountId, var driveId)

    implicitHeight: IKSettings.userDriveRowHeight

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: IKColors.settingsDivider
    }

    Rectangle {
        id: driveBadge

        anchors.left: parent.left
        anchors.leftMargin: IKSettings.userDriveLeadingIndent
        anchors.verticalCenter: parent.verticalCenter
        width: IKSettings.userDriveIconSize
        height: width
        radius: IKRadius.r4
        color: root.color

        IKTintedIcon {
            anchors.fill: parent
            anchors.margins: IKSettings.userDriveIconGlyphInset
            source: "qrc:/assets/onboarding/drive-icon-glyph.svg"
            color: IKColors.settingsDriveGlyph
        }
    }

    Column {
        anchors.left: driveBadge.right
        anchors.leftMargin: IKSpacing.s12
        anchors.right: statusText.left
        anchors.rightMargin: IKSpacing.s16
        anchors.verticalCenter: parent.verticalCenter
        spacing: IKSpacing.s2

        Text {
            id: driveNameText

            width: parent.width
            text: root.name
            color: IKColors.textPrimary
            font.pixelSize: IKFonts.bodySize
            font.weight: IKFonts.emphasized
            elide: Text.ElideRight

            HoverHandler {
                id: driveNameHover
            }

            IKToolTip {
                showRequested: driveNameHover.hovered && driveNameText.truncated
                text: root.name
            }
        }

        Text {
            width: parent.width
            visible: root.hasDistinctAccountName
            text: root.accountName
            color: IKColors.textSecondary
            font.pixelSize: IKFonts.subheadlineSize
            elide: Text.ElideRight
        }
    }

    Text {
        id: statusText

        anchors.right: actionButton.left
        anchors.rightMargin: IKSpacing.s8
        anchors.verticalCenter: parent.verticalCenter
        text: root.isSynchronized ? qsTrId("syncedDrive") : qsTrId("notSyncedDrive")
        color: IKColors.textSecondary
        font.pixelSize: IKFonts.bodySize
        font.weight: IKFonts.medium
    }

    IKModalButton {
        id: actionButton

        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        role: IKModalButton.Tonal
        text: root.isSynchronized ? qsTrId("buttonManage") : qsTrId("buttonEnable")
        actionEnabled: !root.isSynchronized && !root.actionBusy
        busy: root.actionBusy
        Accessible.name: text + " " + root.name
        onClicked: root.activateRequested(actionButton, root.accountId, root.driveId)
    }
}
