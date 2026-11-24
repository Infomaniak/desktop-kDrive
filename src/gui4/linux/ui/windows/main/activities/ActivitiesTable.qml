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

    required property var model
    required property var controller

    readonly property real defaultNameFolderRatio: IKActivities.nameColumnRatio / (IKActivities.nameColumnRatio + IKActivities.folderColumnRatio)
    property real nameFolderRatio: defaultNameFolderRatio

    // Metadata columns are fixed, so they are sized from the widest string they can ever hold in the active locale
    // rather than a constant. The header label counts too, since it elides the same way.
    readonly property real sizeContentWidth: Math.max(root.model.maxTextWidth(root.model.sizeTextSamples, cellFont.font), sizeHeaderMetrics.advanceWidth)
    readonly property real statusContentWidth: Math.max(IKActivities.sourceIconSize + 2 * IKSpacing.s8 + IKActivities.activityIconSize + IKActivities.optionsButtonSize, statusHeaderMetrics.advanceWidth)

    readonly property real fixedColumnsWidth: sizeColumnWidth + statusColumnWidth
    readonly property real flexibleWidth: Math.max(IKActivities.nameColumnMinWidth + IKActivities.folderColumnMinWidth, width - fixedColumnsWidth)
    readonly property real nameColumnWidth: Math.max(IKActivities.nameColumnMinWidth, Math.min(flexibleWidth - IKActivities.folderColumnMinWidth, flexibleWidth * nameFolderRatio))
    readonly property real folderColumnWidth: flexibleWidth - nameColumnWidth
    readonly property real sizeColumnWidth: Math.ceil(sizeContentWidth) + 2 * IKActivities.secondaryCellPadding
    readonly property real statusColumnWidth: Math.ceil(statusContentWidth) + 2 * IKActivities.secondaryCellPadding
    readonly property real contentWidth: nameColumnWidth + folderColumnWidth + sizeColumnWidth + statusColumnWidth

    function resizeNameBoundary(requestedDelta) {
        if (root.flexibleWidth <= 0) {
            return;
        }
        const target = root.nameColumnWidth + requestedDelta;
        const clamped = Math.max(IKActivities.nameColumnMinWidth, Math.min(root.flexibleWidth - IKActivities.folderColumnMinWidth, target));
        root.nameFolderRatio = clamped / root.flexibleWidth;
    }

    function resetNameBoundary() {
        root.nameFolderRatio = root.defaultNameFolderRatio;
    }

    // Measured, never rendered: cell samples use the row font, header labels the header font.
    Text {
        id: cellFont

        visible: false
        font.pixelSize: IKFonts.bodySize
        font.weight: IKFonts.medium
    }
    TextMetrics {
        id: sizeHeaderMetrics

        font.pixelSize: IKFonts.subheadlineSize
        font.weight: IKFonts.medium
        text: qsTrId("labelSize")
    }
    TextMetrics {
        id: statusHeaderMetrics

        font.pixelSize: IKFonts.subheadlineSize
        font.weight: IKFonts.medium
        text: qsTrId("labelStatus")
    }

    clip: true

    ActivitiesTableHeader {
        id: tableHeader

        nameColumnWidth: root.nameColumnWidth
        folderColumnWidth: root.folderColumnWidth
        sizeColumnWidth: root.sizeColumnWidth
        statusColumnWidth: root.statusColumnWidth
        onResizeRequested: delta => root.resizeNameBoundary(delta)
        onResetRequested: root.resetNameBoundary()
    }

    ListView {
        id: listView

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: tableHeader.bottom
        anchors.bottom: parent.bottom
        // A single tab stop that moves a current row, like the folder tree. The current row takes the focus so that
        // screen readers read it: Up, Down, Home and End move it, Return or the Menu key open its options.
        function moveCurrentTo(targetIndex) {
            if (listView.count === 0) {
                return;
            }
            listView.currentIndex = Math.max(0, Math.min(listView.count - 1, targetIndex));
            listView.positionViewAtIndex(listView.currentIndex, ListView.Contain);
            if (listView.currentItem) {
                listView.currentItem.forceActiveFocus();
            }
        }

        function openCurrentOptions(key) {
            const row = listView.currentItem as ActivityRow;
            if (row) {
                row.openOptionsFromKeyboard(key);
            }
        }

        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: root.model
        activeFocusOnTab: true
        keyNavigationEnabled: false
        Accessible.role: Accessible.List
        Accessible.name: qsTrId("tabTitleActivities")

        onActiveFocusChanged: {
            if (listView.activeFocus && !(listView.currentItem && listView.currentItem.activeFocus)) {
                listView.moveCurrentTo(Math.max(0, listView.currentIndex));
            }
        }
        Keys.onUpPressed: listView.moveCurrentTo(listView.currentIndex - 1)
        Keys.onDownPressed: listView.moveCurrentTo(listView.currentIndex + 1)
        Keys.onPressed: event => {
            if (event.key === Qt.Key_Home) {
                listView.moveCurrentTo(0);
            } else if (event.key === Qt.Key_End) {
                listView.moveCurrentTo(listView.count - 1);
            } else if (event.key === Qt.Key_Menu) {
                listView.openCurrentOptions("menu");
            } else if (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier)) {
                listView.openCurrentOptions("shiftF10");
            } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                listView.openCurrentOptions("return");
            } else {
                return;
            }
            event.accepted = true;
        }

        delegate: ActivityRow {
            required property int index
            required property var model

            rowIndex: index
            rowModel: model
            nameColumnWidth: root.nameColumnWidth
            folderColumnWidth: root.folderColumnWidth
            sizeColumnWidth: root.sizeColumnWidth
            statusColumnWidth: root.statusColumnWidth
            menuViewport: listView
            viewportOffset: listView.contentY
            controller: root.controller
        }

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }
    }

    Connections {
        target: root.model

        function onFilterChanged() {
            Qt.callLater(function () {
                listView.positionViewAtBeginning();
            });
        }
    }
}
