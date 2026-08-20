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

Item {
    id: root

    required property int rowIndex
    required property var rowModel
    required property real nameColumnWidth
    required property real folderColumnWidth
    required property real timeColumnWidth
    required property real sizeColumnWidth
    required property real statusColumnWidth
    required property Item menuViewport
    required property real viewportOffset
    required property var controller

    readonly property string rowId: rowModel.rowId
    readonly property string name: rowModel.name
    readonly property string fileIconName: rowModel.fileIconName
    readonly property string actionText: rowModel.actionText
    readonly property string folder: rowModel.folder
    readonly property string timeText: rowModel.timeText
    readonly property string sizeText: rowModel.sizeText
    readonly property bool isDirectory: rowModel.isDirectory
    readonly property int source: rowModel.source
    readonly property int status: rowModel.status
    readonly property int progress: rowModel.progress
    readonly property int availableActions: rowModel.availableActions

    function dismissOptionsMenu() {
        if (optionsMenu && optionsMenu.opened) {
            optionsMenu.close();
        }
    }

    function openOptionsMenu() {
        const rowPosition = root.mapToItem(root.menuViewport, 0, 0);
        const maximumMenuY = Math.max(0, root.menuViewport.height - optionsMenu.height);
        const menuYBelow = rowPosition.y + root.height;
        const menuYAbove = rowPosition.y - optionsMenu.height;
        const menuYInViewport = menuYBelow <= maximumMenuY ? menuYBelow : Math.max(0, menuYAbove);
        optionsMenu.y = Math.min(menuYInViewport, maximumMenuY) - rowPosition.y;
        optionsMenu.open();
    }

    width: nameColumnWidth + folderColumnWidth + timeColumnWidth + sizeColumnWidth + statusColumnWidth
    height: IKActivities.rowHeight
    onRowIdChanged: dismissOptionsMenu()
    onViewportOffsetChanged: dismissOptionsMenu()
    onYChanged: dismissOptionsMenu()

    Rectangle {
        anchors.fill: parent
        radius: IKActivities.rowRadius
        color: root.rowIndex % 2 === 0 ? IKColors.activitiesRowAlternateSurface : "transparent"
    }

    Row {
        anchors.fill: parent

        Item {
            width: root.nameColumnWidth
            height: parent.height

            Row {
                anchors.left: parent.left
                anchors.leftMargin: IKSpacing.s16
                anchors.right: parent.right
                anchors.rightMargin: IKSpacing.s8
                anchors.verticalCenter: parent.verticalCenter
                height: root.actionText.length > 0 ? IKActivities.primaryTextLineHeight * 2 : IKActivities.primaryTextLineHeight
                spacing: IKSpacing.s4

                ActivityFileIcon {
                    anchors.top: parent.top
                    anchors.topMargin: 2
                    fileIconName: root.fileIconName
                    isDirectory: root.isDirectory
                }

                Column {
                    width: Math.max(0, parent.width - x)
                    height: parent.height

                    Text {
                        id: nameText

                        width: parent.width
                        height: IKActivities.primaryTextLineHeight
                        text: root.name
                        color: IKColors.textPrimary
                        font.pixelSize: IKFonts.bodySize
                        font.weight: IKFonts.medium
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter

                        HoverHandler {
                            id: nameHover
                        }

                        IKToolTip {
                            visible: nameHover.hovered && nameText.truncated
                            text: root.name
                        }
                    }

                    Text {
                        width: parent.width
                        height: IKActivities.primaryTextLineHeight
                        visible: root.actionText.length > 0
                        text: root.actionText
                        color: IKColors.textSecondary
                        font.pixelSize: IKFonts.subheadlineSize
                        font.weight: IKFonts.regular
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        Item {
            width: root.folderColumnWidth
            height: root.height

            Text {
                id: folderLink

                anchors.left: parent.left
                anchors.leftMargin: IKActivities.secondaryCellPadding
                anchors.right: parent.right
                anchors.rightMargin: IKActivities.secondaryCellPadding
                anchors.verticalCenter: parent.verticalCenter
                text: root.folder
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.bodySize
                font.weight: IKFonts.medium
                font.underline: true
                elide: Text.ElideLeft
                verticalAlignment: Text.AlignVCenter

                TapHandler {
                    enabled: root.folder.length > 0
                    onTapped: root.controller.openFolder(root.rowId)
                }

                HoverHandler {
                    id: folderHover

                    enabled: root.folder.length > 0
                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                }

                IKToolTip {
                    visible: folderHover.hovered && folderLink.truncated
                    text: root.folder
                }
            }
        }
        SecondaryCell {
            width: root.timeColumnWidth
            text: root.timeText
        }
        SecondaryCell {
            width: root.sizeColumnWidth
            text: root.sizeText
        }

        Item {
            width: root.statusColumnWidth
            height: parent.height

            Row {
                anchors.left: parent.left
                anchors.leftMargin: IKActivities.secondaryCellPadding
                anchors.right: parent.right
                anchors.rightMargin: IKActivities.secondaryCellPadding
                anchors.verticalCenter: parent.verticalCenter
                spacing: IKSpacing.s8

                ActivitySourceIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    sourceType: root.source
                }
                ActivityStatusIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    status: root.status
                    progress: root.progress
                }

                Item {
                    width: Math.max(0, parent.width - x - optionsButton.width)
                    height: 1
                }

                ToolButton {
                    id: optionsButton

                    visible: root.availableActions !== ActivityListModel.NoAvailableAction
                    width: visible ? IKActivities.optionsButtonSize : 0
                    height: IKActivities.optionsButtonSize
                    focusPolicy: visible ? Qt.StrongFocus : Qt.NoFocus
                    hoverEnabled: visible
                    onClicked: root.openOptionsMenu()

                    contentItem: Item {
                        IKTintedIcon {
                            anchors.centerIn: parent
                            width: IKActivities.optionsIconSize
                            height: IKActivities.optionsIconSize
                            source: "qrc:/assets/main/activities/dots-vertical.svg"
                            color: IKColors.textPrimary
                        }
                    }

                    background: Rectangle {
                        radius: IKRadius.r6
                        color: optionsButton.hovered || optionsButton.down ? IKColors.activitiesActionMenuHover : "transparent"
                        border.width: optionsButton.visualFocus ? 2 : 0
                        border.color: IKColors.accentPrimary
                    }

                    Accessible.name: qsTrId("buttonShowOption") + ": " + root.name
                }
            }
        }
    }

    ActivityOptionsMenu {
        id: optionsMenu

        x: Math.max(0, root.width - width)
        y: root.height
        controller: root.controller
        rowId: root.rowId
        availableActions: root.availableActions
        returnFocusItem: optionsButton
    }

    component SecondaryCell: Item {
        id: cell

        required property string text
        height: root.height

        Text {
            id: valueText

            anchors.left: parent.left
            anchors.leftMargin: IKActivities.secondaryCellPadding
            anchors.right: parent.right
            anchors.rightMargin: IKActivities.secondaryCellPadding
            anchors.verticalCenter: parent.verticalCenter
            text: cell.text
            color: IKColors.textSecondary
            font.pixelSize: IKFonts.bodySize
            font.weight: IKFonts.medium
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter

            HoverHandler {
                id: cellHover
            }

            IKToolTip {
                visible: cellHover.hovered && valueText.truncated
                text: cell.text
            }
        }
    }
}
