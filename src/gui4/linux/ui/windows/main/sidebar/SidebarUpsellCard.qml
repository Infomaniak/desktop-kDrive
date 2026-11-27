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

// Upsell card of the main sidebar: my kSuite+ (gradient border, follows the theme) or kSuite Pro (always dark).
Item {
    id: root

    required property int offer
    readonly property bool kSuitePro: offer === UpsellOffers.KSuitePro
    readonly property color foregroundColor: kSuitePro ? IKColors.upsellKSuiteProText : IKColors.upsellMyKSuiteText
    readonly property string actionIdPrefix: "main.sidebar.upsell"

    signal upgradeRequested

    implicitHeight: content.implicitHeight + 2 * IKMainWindow.upsellCardPadding

    IKUpsellBackground {
        anchors.fill: parent
        kSuitePro: root.kSuitePro
    }

    ColumnLayout {
        id: content

        anchors.fill: parent
        anchors.margins: IKMainWindow.upsellCardPadding
        spacing: 0

        IKUpsellLogo {
            kSuitePro: root.kSuitePro
        }
        Text {
            id: description

            Layout.fillWidth: true
            Layout.maximumWidth: IKMainWindow.upsellDescriptionMaxWidth
            Layout.topMargin: IKMainWindow.upsellCardLogoSpacing
            text: root.kSuitePro ? qsTrId("ksuiteProBannerDescription") : qsTrId("myksuitePlusBannerDescription")
            color: root.foregroundColor
            font.pixelSize: IKFonts.bodySize
            wrapMode: Text.Wrap
        }
        IKUpsellButton {
            Layout.topMargin: IKMainWindow.upsellCardActionSpacing
            // A long translation elides instead of widening the card content.
            Layout.maximumWidth: content.width
            kSuitePro: root.kSuitePro
            actionId: root.kSuitePro ? root.actionIdPrefix + ".kSuitePro" : root.actionIdPrefix + ".myKsuitePlus"
            Accessible.description: description.text
            onClicked: root.upgradeRequested()
        }
    }
}
