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

Popup {
    id: root

    required property var controller
    required property Item returnFocusItem

    width: IKActivities.filterMenuWidth
    height: optionsList.contentHeight + IKSpacing.s8 * 2
    padding: IKSpacing.s8
    focus: true
    modal: true
    dim: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onOpened: {
        optionsList.currentIndex = root.controller.filter === ActivityListModel.MyActivityOnly ? 0 : 1;
        Qt.callLater(function () {
            if (optionsList.currentItem) {
                optionsList.currentItem.forceActiveFocus();
            }
        });
    }
    onClosed: root.returnFocusItem.forceActiveFocus()

    function focusOption(index) {
        optionsList.currentIndex = Math.max(0, Math.min(optionsList.count - 1, index));
        Qt.callLater(function () {
            if (optionsList.currentItem) {
                optionsList.currentItem.forceActiveFocus();
            }
        });
    }

    background: Rectangle {
        radius: IKRadius.r12
        color: IKColors.surfaceSecondary
        border.width: 1
        border.color: IKColors.activitiesDivider
    }

    contentItem: ListView {
        id: optionsList

        interactive: false
        model: [
            {
                filter: ActivityListModel.MyActivityOnly,
                label: qsTrId("activitiesTypeMyActivity"),
                icon: "qrc:/assets/main/activities/filter-my-activity.svg"
            },
            {
                filter: ActivityListModel.AllActivities,
                label: qsTrId("activitiesTypeAllActivities"),
                icon: "qrc:/assets/main/activities/filter-all-activities.svg"
            }
        ]

        delegate: ItemDelegate {
            id: option

            required property var modelData
            required property int index

            readonly property bool selected: root.controller.filter === modelData.filter
            readonly property color surfaceColor: hovered || down || visualFocus ? IKColors.surfaceTertiary : "transparent"

            width: optionsList.width
            height: IKActivities.filterMenuOptionHeight
            focusPolicy: Qt.StrongFocus
            hoverEnabled: true
            onClicked: {
                root.controller.filter = modelData.filter;
                root.close();
            }
            Keys.onUpPressed: root.focusOption(option.index - 1)
            Keys.onDownPressed: root.focusOption(option.index + 1)

            contentItem: Row {
                spacing: IKSpacing.s8

                IKTintedIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    width: IKActivities.filterIconSize
                    height: IKActivities.filterIconSize
                    source: option.modelData.icon
                    color: IKColors.textPrimary
                }

                Text {
                    width: Math.max(0, parent.width - x)
                    anchors.verticalCenter: parent.verticalCenter
                    text: option.modelData.label
                    color: IKColors.textPrimary
                    font.pixelSize: IKFonts.bodySize
                    font.weight: IKFonts.emphasized
                    elide: Text.ElideRight
                }
            }

            background: Rectangle {
                radius: IKRadius.r8
                color: option.surfaceColor
                border.width: option.visualFocus ? 2 : 0
                border.color: IKColors.accentPrimary
            }

            Accessible.name: option.modelData.label
            Accessible.selected: option.selected
        }
    }
}
