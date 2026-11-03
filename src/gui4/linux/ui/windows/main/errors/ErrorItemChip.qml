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

// Item an error is about: its icon and name, elided in the middle like a path.
Rectangle {
    id: root

    required property string name
    // Shown in the tooltip: the name alone does not tell two items with the same name apart.
    required property string path
    required property string iconName
    required property bool isDirectory

    // A leading or trailing space is invisible: such a text, a name made of spaces only included, is shown in quotes.
    function quotedIfPadded(text) {
        return /^\s|\s$/.test(text) ? "\"" + text + "\"" : text;
    }

    // From the natural width of the name, so that the chip never depends on its own width; the name is elided only when
    // the owner gives the chip less room.
    implicitWidth: IKActivities.errorsChipIconSize + content.spacing + nameLabel.implicitWidth
                   + 2 * IKActivities.errorsChipHorizontalPadding
    implicitHeight: IKActivities.errorsChipHeight
    radius: IKActivities.errorsChipRadius
    color: IKColors.errorsChipSurface

    Row {
        id: content

        anchors.left: parent.left
        anchors.leftMargin: IKActivities.errorsChipHorizontalPadding
        anchors.verticalCenter: parent.verticalCenter
        spacing: IKActivities.errorsChipSpacing

        ActivityFileIcon {
            anchors.verticalCenter: parent.verticalCenter
            width: IKActivities.errorsChipIconSize
            height: IKActivities.errorsChipIconSize
            fileIconName: root.iconName
            isDirectory: root.isDirectory
        }

        Text {
            id: nameLabel

            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, Math.max(0, root.width - 2 * IKActivities.errorsChipHorizontalPadding
                                                    - IKActivities.errorsChipIconSize - content.spacing))
            text: root.quotedIfPadded(root.name)
            textFormat: Text.PlainText
            color: IKColors.textPrimary
            font.pixelSize: IKFonts.bodySize
            elide: Text.ElideMiddle
        }
    }

    HoverHandler {
        id: hover
    }

    IKToolTip {
        text: root.quotedIfPadded(root.path.length > 0 ? root.path : root.name)
        showRequested: hover.hovered
    }
}
