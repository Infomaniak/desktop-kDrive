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
import QtQuick.Controls
import kDrive.UI

// Compact "Upgrade" push button of an upsell card, with the external-link glyph: it opens the shop in the browser.
// Set `Accessible.description` to the card text: "Upgrade" alone does not name the offer.
Button {
    id: root

    required property bool kSuitePro
    // Stable, language-independent id of the action in the user action log (see UserActionLog).
    required property string actionId
    // Context appended to the log line: ids, booleans or enum values only, never displayed or typed text.
    property var actionDetails: ({})
    readonly property color labelColor: kSuitePro ? IKColors.upsellKSuiteProText : IKColors.upsellMyKSuiteText

    implicitWidth: upgradeLabel.implicitWidth + IKMainWindow.informationBlockExternalIconSpacing
                   + upgradeExternalIcon.width + 2 * IKMainWindow.upsellButtonHorizontalPadding
    implicitHeight: IKMainWindow.upsellButtonHeight
    padding: 0
    leftPadding: IKMainWindow.upsellButtonHorizontalPadding
    rightPadding: IKMainWindow.upsellButtonHorizontalPadding
    text: qsTrId("buttonUpgradeSubscriptionShort")
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    Accessible.role: Accessible.Button
    Accessible.name: text

    onClicked: UserActionLog.action(root.actionId, root.actionDetails)

    contentItem: Item {
        id: upgradeContent

        Row {
            anchors.centerIn: parent
            spacing: IKMainWindow.informationBlockExternalIconSpacing

            Text {
                id: upgradeLabel

                // Width is taken from the content Item rather than from the Row, whose implicit width this feeds.
                width: Math.min(implicitWidth, Math.max(0, upgradeContent.width
                                                        - IKMainWindow.informationBlockExternalIconSpacing
                                                        - upgradeExternalIcon.width))
                text: root.text
                color: root.labelColor
                font.pixelSize: IKFonts.bodySize
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            IKExternalLinkIcon {
                id: upgradeExternalIcon

                anchors.verticalCenter: parent.verticalCenter
                color: root.labelColor
            }
        }
    }

    background: Rectangle {
        radius: IKMainWindow.upsellButtonRadius
        color: root.kSuitePro ? IKColors.upsellKSuiteProButtonSurface : IKColors.upsellMyKSuiteButtonSurface
        opacity: root.down ? IKModalTokens.pressedOpacity : root.hovered ? IKModalTokens.hoverOpacity : 1
        border.width: root.visualFocus ? IKLinkTokens.focusRingWidth : root.kSuitePro ? 0 : IKMainWindow.upsellButtonBorderWidth
        border.color: root.visualFocus ? IKColors.accentPrimary : IKColors.upsellMyKSuiteButtonBorder
    }
}
