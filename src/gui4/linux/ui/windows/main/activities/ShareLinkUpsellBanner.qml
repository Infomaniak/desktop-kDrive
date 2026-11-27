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

// Public link limit notice of the "Share" dialog when the drive can be upgraded: offer logo, title, explanation and
// "Upgrade" button.
Item {
    id: root

    required property bool kSuitePro
    required property string title
    required property string text
    required property string actionId
    readonly property color foregroundColor: kSuitePro ? IKColors.upsellKSuiteProText : IKColors.upsellMyKSuiteText

    signal upgradeRequested

    Layout.fillWidth: true
    implicitHeight: content.implicitHeight + 2 * IKActivities.shareLinkNoticePadding

    IKUpsellBackground {
        anchors.fill: parent
        kSuitePro: root.kSuitePro
        radius: IKActivities.shareLinkNoticeRadius
        surfaceColor: IKColors.shareLinkUpsellSurface
    }

    RowLayout {
        id: content

        anchors.fill: parent
        anchors.margins: IKActivities.shareLinkNoticePadding
        spacing: IKActivities.shareLinkNoticeSpacing

        ColumnLayout {
            Layout.fillWidth: true
            spacing: IKMainWindow.upsellCardLogoSpacing

            IKUpsellLogo {
                kSuitePro: root.kSuitePro
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: IKActivities.shareLinkTextSpacing

                Text {
                    id: titleLabel

                    Layout.fillWidth: true
                    text: root.title
                    textFormat: Text.PlainText
                    color: root.foregroundColor
                    font.pixelSize: IKFonts.bodySize
                    font.weight: IKFonts.medium
                    wrapMode: Text.WordWrap
                    Accessible.role: Accessible.AlertMessage
                    Accessible.name: text
                }
                Text {
                    id: textLabel

                    Layout.fillWidth: true
                    text: root.text
                    textFormat: Text.PlainText
                    color: root.foregroundColor
                    font.pixelSize: IKFonts.subheadlineSize
                    font.weight: IKFonts.medium
                    wrapMode: Text.WordWrap
                }
            }
        }

        IKUpsellButton {
            Layout.alignment: Qt.AlignVCenter
            kSuitePro: root.kSuitePro
            actionId: root.actionId
            actionDetails: ({ offer: root.kSuitePro ? "kSuitePro" : "myKsuitePlus" })
            Accessible.description: titleLabel.text + ". " + textLabel.text
            onClicked: root.upgradeRequested()
        }
    }
}
