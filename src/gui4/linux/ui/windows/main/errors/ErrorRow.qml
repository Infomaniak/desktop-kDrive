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

// One error of the errors page, as a card: title, item, description and the button of its action.
Rectangle {
    id: root

    required property string rowId
    required property string titleId
    required property int titleArgCount
    required property int titleCount
    required property string descriptionId
    required property int descriptionArgCount
    required property string nodeLabelId
    required property string itemName
    required property string itemPath
    required property string itemIconName
    required property bool isDirectory
    required property string actionLabelId
    required property int actionLabelArgCount
    required property bool actionExternal
    required property bool actionPending

    readonly property string title: ErrorTexts.text(root.titleId, root.titleArgCount, root.nodeLabelId, root.titleCount)
    readonly property string description: ErrorTexts.text(root.descriptionId, root.descriptionArgCount,
                                                          root.nodeLabelId, 0)

    signal actionTriggered(string rowId)

    implicitHeight: card.implicitHeight + 2 * IKActivities.errorsCardVerticalPadding
    radius: IKActivities.errorsCardRadius
    color: IKColors.errorsCardSurface
    Accessible.role: Accessible.ListItem
    Accessible.name: root.title
    Accessible.description: root.description

    ColumnLayout {
        id: card

        anchors.fill: parent
        anchors.leftMargin: IKActivities.errorsCardHorizontalPadding
        anchors.rightMargin: IKActivities.errorsCardHorizontalPadding
        anchors.topMargin: IKActivities.errorsCardVerticalPadding
        anchors.bottomMargin: IKActivities.errorsCardVerticalPadding
        spacing: IKActivities.errorsCardContentSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: IKActivities.errorsCardActionSpacing

            ColumnLayout {
                Layout.fillWidth: true
                spacing: IKActivities.errorsCardContentSpacing

                Text {
                    Layout.fillWidth: true
                    text: root.title
                    textFormat: Text.PlainText
                    color: IKColors.textPrimary
                    font.pixelSize: IKFonts.bodySize
                    font.weight: IKFonts.emphasized
                    wrapMode: Text.WordWrap
                    Accessible.role: Accessible.StaticText
                    Accessible.name: text
                }

                ErrorItemChip {
                    Layout.maximumWidth: card.width * IKActivities.errorsChipMaximumWidthRatio
                    visible: root.itemName.length > 0
                    name: root.itemName
                    path: root.itemPath
                    iconName: root.itemIconName
                    isDirectory: root.isDirectory
                }

                Text {
                    Layout.fillWidth: true
                    text: root.description
                    textFormat: Text.PlainText
                    color: IKColors.textSecondary
                    font.pixelSize: IKFonts.subheadlineSize
                    wrapMode: Text.WordWrap
                    Accessible.role: Accessible.StaticText
                    Accessible.name: text
                }
            }

            IKModalButton {
                id: actionButton

                Layout.alignment: Qt.AlignVCenter
                implicitHeight: IKActivities.errorsActionButtonHeight
                visible: root.actionLabelId.length > 0
                role: IKModalButton.Tonal
                text: ErrorTexts.text(root.actionLabelId, root.actionLabelArgCount, root.nodeLabelId, 0)
                external: root.actionExternal
                busy: root.actionPending
                // Several rows can share a label ("Rename file"): the item tells them apart.
                Accessible.description: root.itemName.length > 0 ? root.itemName : root.title
                onClicked: root.actionTriggered(root.rowId)
            }
        }
    }
}
