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
import QtQuick.Layouts
import kDrive.UI

// Card of one link of the "Share" dialog: icon, title, description, state text and "Copy" button. Notices about the
// link go below the row, inside the card.
Rectangle {
    id: root

    required property string title
    required property string description
    required property url iconSource
    // Shown at the left of the button, empty when the link has nothing to report.
    property string statusText
    property bool copyEnabled: true
    // The link is being created: the button shows a spinner instead of its label.
    property bool copyBusy: false
    required property string actionId
    // Whether one of the notices placed below the row is shown: an empty column would still add the card spacing.
    property bool hasNotice: false
    default property alias noticeData: noticeColumn.data

    signal copyRequested

    implicitHeight: content.implicitHeight + 2 * IKActivities.shareLinkCardPadding
    radius: IKActivities.shareLinkCardRadius
    color: IKColors.shareLinkCardSurface

    ColumnLayout {
        id: content

        anchors.fill: parent
        anchors.margins: IKActivities.shareLinkCardPadding
        spacing: IKActivities.shareLinkCardContentSpacing

        RowLayout {
            Layout.fillWidth: true
            Layout.minimumHeight: IKActivities.shareLinkRowMinHeight
            spacing: IKActivities.shareLinkRowSpacing

            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: IKActivities.shareLinkIconSpacing

                IKTintedIcon {
                    Layout.alignment: Qt.AlignTop
                    Layout.topMargin: IKSpacing.s2
                    Layout.preferredWidth: IKActivities.shareLinkIconSize
                    Layout.preferredHeight: IKActivities.shareLinkIconSize
                    source: root.iconSource
                    color: IKColors.shareLinkIcon
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
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.description
                        textFormat: Text.PlainText
                        color: IKColors.textSecondary
                        font.pixelSize: IKFonts.subheadlineSize
                        font.weight: IKFonts.medium
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Text {
                id: statusLabel

                Layout.alignment: Qt.AlignVCenter
                Layout.maximumWidth: root.width / 3
                visible: root.statusText.length > 0
                text: root.statusText
                textFormat: Text.PlainText
                color: IKColors.shareLinkStatusText
                font.pixelSize: IKFonts.bodySize
                font.weight: IKFonts.medium
                horizontalAlignment: Text.AlignRight
                wrapMode: Text.WordWrap
                // Announced when the state changes: the user stays on the button.
                Accessible.role: Accessible.AlertMessage
                Accessible.name: text
            }

            IKModalButton {
                Layout.alignment: Qt.AlignVCenter
                implicitHeight: IKActivities.shareLinkButtonHeight
                role: IKModalButton.Tonal
                text: qsTrId("menuCopy")
                actionEnabled: root.copyEnabled
                busy: root.copyBusy
                // Both buttons read "Copy": the link title tells them apart.
                Accessible.description: root.title
                actionId: root.actionId
                onClicked: root.copyRequested()
            }
        }

        ColumnLayout {
            id: noticeColumn

            Layout.fillWidth: true
            visible: root.hasNotice
            spacing: IKActivities.shareLinkCardSpacing
        }
    }
}
