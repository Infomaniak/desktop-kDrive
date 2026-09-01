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
import QtQuick.Controls
import kDrive.UI

// Inline textual action. It carries no surface of its own so it can sit directly on a card without competing with the
// card background, unlike IKModalButton which owns a filled or outlined shape.
Button {
    id: root

    property bool actionEnabled: true

    enabled: actionEnabled
    padding: 0
    implicitWidth: linkText.implicitWidth
    implicitHeight: Math.max(linkText.implicitHeight, IKLinkTokens.minimumHeight)
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    Accessible.role: Accessible.Button
    Accessible.name: text

    // The control sizes background and contentItem itself, so neither is anchored here.
    background: Rectangle {
        radius: IKRadius.r4
        color: "transparent"
        border.width: root.visualFocus ? IKLinkTokens.focusRingWidth : 0
        border.color: IKColors.accentPrimary
    }

    contentItem: Text {
        id: linkText

        text: root.text
        color: root.enabled ? IKColors.actionPrimary : IKColors.actionDisabled
        font.pixelSize: IKFonts.bodySize
        font.underline: root.hovered || root.down
        horizontalAlignment: Text.AlignLeft
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
