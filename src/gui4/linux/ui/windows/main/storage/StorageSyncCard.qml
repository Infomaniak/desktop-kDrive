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

// Points to the synchronized folder selection, the way to free up space on a fully downloaded synchronization.
Rectangle {
    id: root

    signal manageRequested

    color: IKColors.storageActionSurface
    radius: IKRadius.r12
    implicitHeight: Math.max(IKStorage.rowHeight, textColumn.implicitHeight + 2 * IKSpacing.s4)

    Column {
        id: textColumn

        anchors.left: parent.left
        anchors.leftMargin: IKStorage.cardHorizontalPadding
        anchors.right: manageButton.left
        anchors.rightMargin: IKSpacing.s16
        anchors.verticalCenter: parent.verticalCenter
        spacing: IKStorage.actionTextSpacing

        Text {
            width: parent.width
            text: qsTrId("storageSyncBlockTitle")
            color: IKColors.textPrimary
            font.pixelSize: IKFonts.bodySize
            font.weight: IKFonts.medium
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            text: qsTrId("storageSyncBlockDescription")
            color: IKColors.textSecondary
            font.pixelSize: IKFonts.subheadlineSize
            lineHeight: IKStorage.actionSubtitleLineHeight
            lineHeightMode: Text.FixedHeight
            wrapMode: Text.WordWrap
        }
    }

    IKLinkButton {
        id: manageButton

        anchors.right: parent.right
        anchors.rightMargin: IKStorage.cardHorizontalPadding
        anchors.verticalCenter: parent.verticalCenter
        text: qsTrId("titleManageSynchronization")

        onClicked: root.manageRequested()
    }
}
