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

    // Metadata columns are fixed, so they are sized from the widest string they can ever hold in the active locale
    // rather than a constant: the longest wordings differ enough between languages to truncate otherwise. The header
    // label counts too, since it elides the same way, and it is the binding constraint for size and status.
    readonly property real timeContentWidth: Math.max(root.model.maxTextWidth(root.model.timeTextSamples, cellFont.font), timeHeaderMetrics.advanceWidth)
    readonly property real sizeContentWidth: Math.max(root.model.maxTextWidth(root.model.sizeTextSamples, cellFont.font), sizeHeaderMetrics.advanceWidth)
    readonly property real statusContentWidth: Math.max(IKActivities.sourceIconSize + 2 * IKSpacing.s8 + IKActivities.activityIconSize + IKActivities.optionsButtonSize, statusHeaderMetrics.advanceWidth)

    readonly property real fixedColumnsWidth: timeColumnWidth + sizeColumnWidth + statusColumnWidth
    readonly property real flexibleWidth: Math.max(IKActivities.nameColumnMinWidth + IKActivities.folderColumnMinWidth, width - fixedColumnsWidth)
    readonly property real nameColumnWidth: Math.max(IKActivities.nameColumnMinWidth, Math.min(flexibleWidth - IKActivities.folderColumnMinWidth, flexibleWidth * nameFolderRatio))
    readonly property real folderColumnWidth: flexibleWidth - nameColumnWidth
    readonly property real timeColumnWidth: Math.ceil(timeContentWidth) + 2 * IKActivities.secondaryCellPadding
    readonly property real sizeColumnWidth: Math.ceil(sizeContentWidth) + 2 * IKActivities.secondaryCellPadding
    readonly property real statusColumnWidth: Math.ceil(statusContentWidth) + 2 * IKActivities.secondaryCellPadding
    readonly property real contentWidth: nameColumnWidth + folderColumnWidth + timeColumnWidth + sizeColumnWidth + statusColumnWidth

    function resizeNameBoundary(requestedDelta) {
        if (root.flexibleWidth <= 0) {
            return;
        }
        const target = root.nameColumnWidth + requestedDelta;
        const clamped = Math.max(IKActivities.nameColumnMinWidth, Math.min(root.flexibleWidth - IKActivities.folderColumnMinWidth, target));
        root.nameFolderRatio = clamped / root.flexibleWidth;
    }

    // Measured, never rendered: cell samples use the row font, header labels the header font.
    Text {
        id: cellFont

        visible: false
        font.pixelSize: IKFonts.bodySize
        font.weight: IKFonts.medium
    }
    TextMetrics {
        id: timeHeaderMetrics

        font.pixelSize: IKFonts.subheadlineSize
        font.weight: IKFonts.medium
        text: qsTrId("labelTime")
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
