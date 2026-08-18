/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import kDrive.UI

Item {
    id: root

    required property var model
    required property var controller

    property real nameFolderRatio: IKActivities.nameColumnRatio / (IKActivities.nameColumnRatio + IKActivities.folderColumnRatio)

    readonly property real fixedColumnsWidth: IKActivities.timeColumnWidth + IKActivities.sizeColumnWidth + IKActivities.statusColumnWidth
    readonly property real flexibleWidth: Math.max(IKActivities.nameColumnMinWidth + IKActivities.folderColumnMinWidth, width - fixedColumnsWidth)
    readonly property real nameColumnWidth: Math.max(IKActivities.nameColumnMinWidth, Math.min(flexibleWidth - IKActivities.folderColumnMinWidth, flexibleWidth * nameFolderRatio))
    readonly property real folderColumnWidth: flexibleWidth - nameColumnWidth
    readonly property real timeColumnWidth: IKActivities.timeColumnWidth
    readonly property real sizeColumnWidth: IKActivities.sizeColumnWidth
    readonly property real statusColumnWidth: IKActivities.statusColumnWidth
    readonly property real contentWidth: nameColumnWidth + folderColumnWidth + timeColumnWidth + sizeColumnWidth + statusColumnWidth

    function resizeNameBoundary(requestedDelta) {
        if (root.flexibleWidth <= 0) {
            return;
        }
        const target = root.nameColumnWidth + requestedDelta;
        const clamped = Math.max(IKActivities.nameColumnMinWidth, Math.min(root.flexibleWidth - IKActivities.folderColumnMinWidth, target));
        root.nameFolderRatio = clamped / root.flexibleWidth;
    }

    clip: true

    ActivitiesTableHeader {
        id: tableHeader

        nameColumnWidth: root.nameColumnWidth
        folderColumnWidth: root.folderColumnWidth
        timeColumnWidth: root.timeColumnWidth
        sizeColumnWidth: root.sizeColumnWidth
        statusColumnWidth: root.statusColumnWidth
        onResizeRequested: delta => root.resizeNameBoundary(delta)
    }

    ListView {
        id: listView

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: tableHeader.bottom
        anchors.bottom: parent.bottom
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: root.model

        delegate: ActivityRow {
            required property int index
            required property var model

            rowIndex: index
            rowModel: model
            nameColumnWidth: root.nameColumnWidth
            folderColumnWidth: root.folderColumnWidth
            timeColumnWidth: root.timeColumnWidth
            sizeColumnWidth: root.sizeColumnWidth
            statusColumnWidth: root.statusColumnWidth
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
