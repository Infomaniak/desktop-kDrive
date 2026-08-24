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
import QtQuick.Effects
import kDrive.UI

Rectangle {
    id: root

    required property var controller
    required property bool loading

    clip: true
    color: IKColors.storageSurface
    height: IKStorage.cardHeight
    radius: IKRadius.r12

    Item {
        id: header

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: IKStorage.headerHeight

        Text {
            anchors.left: parent.left
            anchors.leftMargin: IKStorage.cardHorizontalPadding
            anchors.right: root.loading ? loadingIndicator.left : usageText.left
            anchors.rightMargin: IKSpacing.s16
            color: IKColors.textPrimary
            elide: Text.ElideRight
            font.pixelSize: IKFonts.bodySize
            font.weight: IKFonts.medium
            text: root.controller.volumeName
            visible: !root.loading
            y: IKSpacing.s16
        }
        Text {
            id: usageText

            anchors.right: parent.right
            anchors.rightMargin: IKStorage.cardHorizontalPadding
            color: IKColors.textSecondary
            font.pixelSize: IKFonts.bodySize
            font.weight: IKFonts.medium
            text: root.controller.usageText
            visible: !root.loading
            y: IKSpacing.s16
        }
        IKLoadingSpinner {
            id: loadingIndicator

            Accessible.name: qsTrId("storageLoadingHint")
            anchors.right: parent.right
            anchors.rightMargin: IKStorage.cardHorizontalPadding
            height: IKStorage.loadingSpinnerSize
            visible: root.loading
            width: IKStorage.loadingSpinnerSize
            y: IKSpacing.s16
        }
        Rectangle {
            id: storageBar

            readonly property real displayedRatioTotal: rawSyncRatio + rawOtherRatio + rawAvailableRatio
            readonly property real otherWidth: width * rawOtherRatio / displayedRatioTotal
            readonly property real rawAvailableRatio: root.controller.availableRatio
            readonly property real rawOtherRatio: Math.max(root.controller.otherRatio, IKStorage.minimumSegmentRatio)
            readonly property real rawSyncRatio: Math.max(root.controller.syncRatio, IKStorage.minimumSegmentRatio)
            readonly property real syncWidth: width * rawSyncRatio / displayedRatioTotal

            anchors.left: parent.left
            anchors.leftMargin: IKStorage.cardHorizontalPadding
            anchors.right: parent.right
            anchors.rightMargin: IKStorage.cardHorizontalPadding
            clip: true
            color: root.loading ? IKColors.surfaceTertiary : IKColors.storageFree
            height: IKStorage.storageBarHeight
            layer.enabled: true
            radius: IKStorage.storageBarRadius
            y: IKSpacing.s16 + IKFonts.bodySize + IKSpacing.s12

            layer.effect: MultiEffect {
                maskEnabled: true
                maskSource: storageBarMask
            }

            Rectangle {
                anchors.left: parent.left
                color: IKColors.storageBarSync
                height: parent.height
                width: root.loading ? 0 : storageBar.syncWidth
            }
            Rectangle {
                color: IKColors.storageBarOther
                height: parent.height
                width: root.loading ? 0 : storageBar.otherWidth
                x: storageBar.syncWidth
            }
            Rectangle {
                color: IKColors.storageBarSeparator
                height: parent.height
                visible: !root.loading
                width: 1
                x: Math.max(0, storageBar.syncWidth - width)
            }
            Rectangle {
                color: IKColors.storageBarSeparator
                height: parent.height
                visible: !root.loading && root.controller.availableRatio > 0
                width: 1
                x: Math.max(0, storageBar.syncWidth + storageBar.otherWidth - width)
            }
        }
        Rectangle {
            id: storageBarMask

            color: IKColors.storageFree
            height: storageBar.height
            layer.enabled: true
            radius: IKStorage.storageBarRadius
            visible: false
            width: storageBar.width
        }
    }
    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom

        LegendRow {
            dotColor: IKColors.storageLegendSync
            height: IKStorage.rowHeight
            label: qsTrId("storageComputerUsedByKDrive")
            loading: root.loading
            value: root.controller.syncSizeText
            width: parent.width
        }
        LegendRow {
            dotColor: IKColors.storageLegendOther
            height: IKStorage.rowHeight
            label: qsTrId("storageComputerUsedByOther")
            loading: root.loading
            value: root.controller.otherSizeText
            width: parent.width
        }
        LegendRow {
            dotColor: IKColors.storageFree
            height: IKStorage.rowHeight
            label: qsTrId("storageComputerFreeSpace")
            loading: root.loading
            value: root.controller.availableSizeText
            width: parent.width
        }
    }

    component LegendRow: Item {
        id: legendRow

        required property color dotColor
        required property string label
        required property bool loading
        required property string value

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            color: IKColors.storageDivider
            height: 1
        }
        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: IKStorage.cardHorizontalPadding
            anchors.verticalCenter: parent.verticalCenter
            color: legendRow.dotColor
            height: IKStorage.legendDotSize
            radius: width / 2
            width: IKStorage.legendDotSize
        }
        Text {
            anchors.left: parent.left
            anchors.leftMargin: IKStorage.cardHorizontalPadding + IKStorage.legendDotSize + IKSpacing.s8
            anchors.right: valueLoader.left
            anchors.rightMargin: IKSpacing.s16
            anchors.verticalCenter: parent.verticalCenter
            color: IKColors.textPrimary
            elide: Text.ElideRight
            font.pixelSize: IKFonts.bodySize
            font.weight: IKFonts.medium
            text: legendRow.label
        }
        Item {
            id: valueLoader

            anchors.right: parent.right
            anchors.rightMargin: IKStorage.cardHorizontalPadding
            anchors.verticalCenter: parent.verticalCenter
            height: IKFonts.bodySize + IKSpacing.s4
            width: legendRow.loading ? IKStorage.loadingPlaceholderWidth : valueText.implicitWidth

            Rectangle {
                anchors.centerIn: parent
                color: IKColors.surfaceTertiary
                height: IKStorage.loadingPlaceholderHeight
                radius: IKRadius.r4
                visible: legendRow.loading
                width: IKStorage.loadingPlaceholderWidth
            }
            Text {
                id: valueText

                anchors.centerIn: parent
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.bodySize
                font.weight: IKFonts.medium
                text: legendRow.value
                visible: !legendRow.loading
            }
        }
    }
}
