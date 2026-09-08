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

IKModal {
    id: root

    required property string cancelText
    required property string confirmText
    required property string description
    property bool busy: false
    property int confirmRole: IKModalButton.Primary
    readonly property real requiredActionsWidth: cancelButton.implicitWidth + confirmButton.implicitWidth
                                                 + IKModalTokens.actionSpacing

    signal confirmed
    signal dismissed

    function dismiss() {
        if (root.busy) {
            return;
        }

        root.close();
        root.dismissed();
    }

    actionsStacked: requiredActionsWidth > availableWidth
    escapeDismissible: !busy
    initialFocusItem: cancelButton

    bodyData: Text {
        color: IKColors.textSecondary
        font.pixelSize: IKFonts.bodySize
        lineHeight: IKFonts.title2Size
        lineHeightMode: Text.FixedHeight
        text: root.description
        width: parent.width
        wrapMode: Text.WordWrap
    }
    footerData: [
        IKModalButton {
            id: cancelButton

            Layout.fillWidth: root.actionsStacked
            actionEnabled: !root.busy
            role: IKModalButton.Secondary
            text: root.cancelText

            onClicked: root.dismiss()
        },
        IKModalButton {
            id: confirmButton

            Layout.fillWidth: root.actionsStacked
            actionEnabled: !root.busy
            busy: root.busy
            role: root.confirmRole
            text: root.confirmText

            onClicked: root.confirmed()
        }
    ]

    onDismissRequested: dismiss()
}
