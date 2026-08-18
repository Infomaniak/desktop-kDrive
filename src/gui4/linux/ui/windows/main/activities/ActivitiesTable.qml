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

    property var columnRatios: [
        IKActivities.nameColumnRatio,
        IKActivities.folderColumnRatio,
        IKActivities.timeColumnRatio,
        IKActivities.sizeColumnRatio,
        IKActivities.statusColumnRatio
    ]

    readonly property var minimumColumnWidths: [
        IKActivities.nameColumnMinWidth,
        IKActivities.folderColumnMinWidth,
        IKActivities.timeColumnMinWidth,
        IKActivities.sizeColumnMinWidth,
        IKActivities.statusColumnMinWidth
    ]
    readonly property var columnWidths: resolveColumnWidths(width)
    readonly property real nameColumnWidth: columnWidths[0]
    readonly property real folderColumnWidth: columnWidths[1]
    readonly property real timeColumnWidth: columnWidths[2]
    readonly property real sizeColumnWidth: columnWidths[3]
    readonly property real statusColumnWidth: columnWidths[4]
    readonly property real contentWidth: nameColumnWidth + folderColumnWidth + timeColumnWidth + sizeColumnWidth + statusColumnWidth

    function resolveColumnWidths(availableWidth) {
        const ratios = root.columnRatios;
        const minimumWidths = root.minimumColumnWidths;
        const widths = new Array(ratios.length).fill(0);
        const flexible = new Array(ratios.length).fill(true);
        let remainingWidth = Math.max(0, availableWidth);
        let remainingRatio = ratios.reduce((sum, ratio) => sum + Math.max(0, ratio), 0);
        let minimumApplied = true;

        while (minimumApplied) {
            minimumApplied = false;
            for (let index = 0; index < widths.length; ++index) {
                if (!flexible[index]) {
                    continue;
                }
                const ratio = Math.max(0, ratios[index]);
                const proportionalWidth = remainingRatio > 0 ? remainingWidth * ratio / remainingRatio : 0;
                if (proportionalWidth >= minimumWidths[index]) {
                    continue;
                }
                widths[index] = minimumWidths[index];
                flexible[index] = false;
                remainingWidth -= minimumWidths[index];
                remainingRatio -= ratio;
                minimumApplied = true;
            }
        }

        for (let index = 0; index < widths.length; ++index) {
            if (flexible[index]) {
                widths[index] = remainingWidth * ratios[index] / remainingRatio;
            }
        }
        return widths;
    }

    function resizeBoundary(boundaryIndex, requestedDelta) {
        if (boundaryIndex < 0 || boundaryIndex >= root.columnWidths.length - 1 || root.width <= 0) {
            return;
        }
        const minimumWidths = [IKActivities.nameColumnMinWidth, IKActivities.folderColumnMinWidth, IKActivities.timeColumnMinWidth, IKActivities.sizeColumnMinWidth, IKActivities.statusColumnMinWidth];
        const widths = root.columnWidths.slice();
        const leftIndex = boundaryIndex;
        const rightIndex = boundaryIndex + 1;
        const minimumDelta = minimumWidths[leftIndex] - widths[leftIndex];
        const maximumDelta = widths[rightIndex] - minimumWidths[rightIndex];
        const delta = Math.max(minimumDelta, Math.min(maximumDelta, requestedDelta));
        widths[leftIndex] += delta;
        widths[rightIndex] -= delta;

        root.nameColumnRatio = widths[0] / root.width;
        root.folderColumnRatio = widths[1] / root.width;
        root.timeColumnRatio = widths[2] / root.width;
        root.sizeColumnRatio = widths[3] / root.width;
        root.statusColumnRatio = widths[4] / root.width;
    }

    clip: true

    ActivitiesTableHeader {
        id: tableHeader

        nameColumnWidth: root.nameColumnWidth
        folderColumnWidth: root.folderColumnWidth
        timeColumnWidth: root.timeColumnWidth
        sizeColumnWidth: root.sizeColumnWidth
        statusColumnWidth: root.statusColumnWidth
        onBoundaryDragged: (boundaryIndex, delta) => root.resizeBoundary(boundaryIndex, delta)
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
