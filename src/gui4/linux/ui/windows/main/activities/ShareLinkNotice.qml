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

import QtQuick
import QtQuick.Layouts
import kDrive.UI

// Warning notice of the "Share" dialog, below the link it is about: icon tile, title and optional explanation.
Rectangle {
    id: root

    required property string title
    property string text
    required property url iconSource

    Layout.fillWidth: true
    implicitHeight: content.implicitHeight + 2 * IKActivities.shareLinkNoticePadding
    radius: IKActivities.shareLinkNoticeRadius
    color: IKColors.shareLinkNoticeSurface
    border.width: IKActivities.shareLinkNoticeBorderWidth
    border.color: IKColors.shareLinkNoticeBorder
    Accessible.role: Accessible.AlertMessage
    Accessible.name: root.title
    Accessible.description: root.text

    RowLayout {
        id: content

        anchors.fill: parent
        anchors.margins: IKActivities.shareLinkNoticePadding
        spacing: IKActivities.shareLinkNoticeSpacing

        Rectangle {
            // Aligned on the title, or centered on it when there is no explanation.
            Layout.alignment: root.text.length > 0 ? Qt.AlignTop : Qt.AlignVCenter
            Layout.preferredWidth: IKActivities.shareLinkNoticeTileSize
            Layout.preferredHeight: IKActivities.shareLinkNoticeTileSize
            radius: IKActivities.shareLinkNoticeTileRadius
            color: IKColors.shareLinkNoticeIconTile

            IKTintedIcon {
                anchors.centerIn: parent
                width: IKActivities.shareLinkNoticeGlyphSize
                height: IKActivities.shareLinkNoticeGlyphSize
                source: root.iconSource
                color: IKColors.shareLinkNoticeGlyph
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: IKActivities.shareLinkTextSpacing

            Text {
                Layout.fillWidth: true
                text: root.title
                textFormat: Text.PlainText
                color: IKColors.textPrimary
                font.pixelSize: IKFonts.bodySize
                font.weight: IKFonts.medium
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                visible: root.text.length > 0
                text: root.text
                textFormat: Text.PlainText
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.subheadlineSize
                font.weight: IKFonts.medium
                wrapMode: Text.WordWrap
            }
        }
    }
}
