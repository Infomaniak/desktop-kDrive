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
import QtQuick.Layouts
import kDrive.UI

// Read-only field of the errors page with a copy button: a path in its display form, or a command. The owner copies
// the value itself on `copyRequested`, for instance the absolute form of the path.
Rectangle {
    id: root

    required property string value
    property string copyText: qsTrId("buttonCopyPath")
    property string copiedText: qsTrId("pathCopiedToClipboard")
    // Wraps the value instead of eliding it, for a command the user must read in full before running it.
    property bool wrapsValue: false

    signal copyRequested

    implicitHeight: root.wrapsValue
                    ? Math.max(IKActivities.errorsCopyFieldHeight,
                               pathLabel.implicitHeight + 2 * IKActivities.errorsCopyFieldWrappedPadding)
                    : IKActivities.errorsCopyFieldHeight
    radius: IKRadius.r4
    color: IKColors.errorsCopyFieldSurface
    border.width: 1
    border.color: IKColors.errorsCopyFieldBorder

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: IKSpacing.s8
        anchors.rightMargin: IKSpacing.s4
        anchors.topMargin: root.wrapsValue ? IKActivities.errorsCopyFieldWrappedPadding : 0
        anchors.bottomMargin: root.wrapsValue ? IKActivities.errorsCopyFieldWrappedPadding : 0
        spacing: IKSpacing.s4

        Text {
            id: pathLabel

            Layout.fillWidth: true
            text: root.value
            textFormat: Text.PlainText
            color: IKColors.textSecondary
            font.pixelSize: IKFonts.bodySize
            wrapMode: root.wrapsValue ? Text.WrapAnywhere : Text.NoWrap
            elide: root.wrapsValue ? Text.ElideNone : Text.ElideMiddle

            HoverHandler {
                id: pathHover
            }

            IKToolTip {
                text: root.value
                showRequested: pathHover.hovered && pathLabel.truncated
            }
        }

        ErrorCopyButton {
            text: root.copyText
            copiedText: root.copiedText
            onCopyRequested: root.copyRequested()
        }
    }
}
