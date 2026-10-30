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

// A sentence of the explanation dialog whose `%1` placeholder is a link, such as "See the %1 to restore access" with
// "FAQ". The sentence takes the keyboard focus as a whole and Enter or Space follows its link, since the link of a rich
// text cannot be reached with Tab.
Text {
    id: root

    // Translated sentence holding one `%1`, and the text of the link that replaces it.
    required property string sentence
    required property string linkText

    readonly property int placeholderIndex: root.sentence.indexOf("%1")
    readonly property string plainText: root.placeholderIndex < 0 ? root.sentence : root.sentence.replace("%1", root.linkText)

    // The translations are plain text: escape them before building the markup around the link.
    function escaped(value) {
        return value.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
    }

    signal activated

    text: root.placeholderIndex < 0
          ? root.escaped(root.sentence)
          : root.escaped(root.sentence.slice(0, root.placeholderIndex)) + "<a href=\"link\">" + root.escaped(root.linkText)
            + "</a>" + root.escaped(root.sentence.slice(root.placeholderIndex + 2))
    textFormat: Text.StyledText
    linkColor: IKColors.actionPrimary
    color: IKColors.textPrimary
    font.pixelSize: IKFonts.bodySize
    font.underline: root.activeFocus
    wrapMode: Text.WordWrap
    activeFocusOnTab: root.placeholderIndex >= 0
    // A sentence without link is plain text for screen readers.
    Accessible.role: root.placeholderIndex >= 0 ? Accessible.Link : Accessible.StaticText
    Accessible.name: root.plainText
    Accessible.onPressAction: root.activated()
    Keys.onReturnPressed: root.activated()
    Keys.onEnterPressed: root.activated()
    Keys.onSpacePressed: root.activated()
    onLinkActivated: root.activated()

    HoverHandler {
        cursorShape: root.hoveredLink.length > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
