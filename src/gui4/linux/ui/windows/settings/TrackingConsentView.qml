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
import QtQuick.Controls
import kDrive.UI

ScrollView {
    id: root

    required property var controller
    required property string navigationTitle
    required property url logoSource
    required property string description
    required property string errorText
    required property bool trackingEnabled
    required property var toggleAction

    contentWidth: availableWidth
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    Column {
        width: root.availableWidth
        padding: IKSettings.pageMargin
        spacing: IKSpacing.s24

        Image {
            width: Math.min(sourceSize.width, parent.width - 2 * IKSettings.pageMargin)
            height: sourceSize.width > 0 ? sourceSize.height * width / sourceSize.width : 0
            source: root.logoSource
            fillMode: Image.PreserveAspectFit
            Accessible.ignored: true
        }

        Text {
            width: parent.width - 2 * IKSettings.pageMargin
            text: root.description
            color: IKColors.textSecondary
            font.pixelSize: IKFonts.bodySize
            wrapMode: Text.WordWrap
        }

        SettingsGroup {
            width: parent.width - 2 * IKSettings.pageMargin

            SettingsRow {
                title: qsTrId("labelAllowTracking")
                separator: false

                IKSwitch {
                    text: qsTrId("labelAllowTracking")
                    value: root.trackingEnabled
                    enabled: root.controller.ready && !root.controller.saving
                    onToggleRequested: value => root.toggleAction(value)
                }
            }
        }

        Text {
            width: parent.width - 2 * IKSettings.pageMargin
            visible: root.errorText.length > 0
            text: root.errorText
            color: IKColors.statusStrongWarning
            font.pixelSize: IKFonts.bodySize
            wrapMode: Text.WordWrap
            Accessible.role: Accessible.AlertMessage
        }
    }
}
