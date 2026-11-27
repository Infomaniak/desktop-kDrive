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
import kDrive.UI

// Logo of the offer of an upsell card, sized by height: my kSuite+ follows the theme, kSuite Pro is drawn on its dark
// surface.
Item {
    id: root

    required property bool kSuitePro

    // The image keeps its natural size, the item scales it to the logo height.
    implicitHeight: IKMainWindow.upsellLogoHeight
    implicitWidth: IKMainWindow.upsellLogoHeight * logo.implicitWidth / Math.max(1, logo.implicitHeight)
    Accessible.role: Accessible.Graphic
    Accessible.name: root.kSuitePro ? "kSuite Pro" : "my kSuite+"

    Image {
        id: logo

        anchors.fill: parent
        source: {
            if (root.kSuitePro) {
                return "qrc:/assets/main/upsell/ksuite-pro-logo.svg";
            }
            return IKColors.darkMode ? "qrc:/assets/main/upsell/ksuite-plus-logo-dark.svg"
                                     : "qrc:/assets/main/upsell/ksuite-plus-logo.svg";
        }
        sourceSize.height: 3 * IKMainWindow.upsellLogoHeight
        fillMode: Image.PreserveAspectFit
    }
}
