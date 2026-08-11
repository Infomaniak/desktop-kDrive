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

Text {
    id: root

    required property var controller

    function escaped(value) {
        return value.replace(/&/g, "&amp;")
                    .replace(/</g, "&lt;")
                    .replace(/>/g, "&gt;")
                    .replace(/"/g, "&quot;")
                    .replace(/'/g, "&#39;")
    }

    function statusLabel() {
        switch (controller.status) {
        case HomeController.Syncing:
            return qsTrId("synchroInProgress")
        case HomeController.Paused:
            return qsTrId("synchroPaused")
        case HomeController.Offline:
            return qsTrId("synchroPaused")
        default:
            return qsTrId("synchroUpToDate")
        }
    }

    text: qsTrId("greetingLabel")
              .arg(root.escaped(controller.firstName))
              .arg("<b>" + root.escaped(root.statusLabel()) + "</b>")
    textFormat: Text.StyledText
    color: IKColors.textPrimary
    font.pixelSize: IKFonts.title3Size
    lineHeightMode: Text.FixedHeight
    lineHeight: 20
    wrapMode: Text.WordWrap
    elide: Text.ElideRight
}
