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

// Usage card of the Storage page: a title, its usage, a gauge of colored segments over the free space, and a legend.
Rectangle {
    id: root

    required property string title
    property string usageText
    property bool loading: false
    // Animates segment widths; values restored from a cache or a first presentation appear at once.
    property bool animated: false
    property real availableRatio: 0
    property list<StorageSegment> segments
    property list<StorageLegendEntry> entries
    // Optional trailing item of the header, after the usage text.
    property Component headerAccessory: null

    function displayedRatio(segment) {
        // A segment that occupies nothing takes no width: the minimum only keeps a tiny occupation visible.
        return segment.measured && segment.ratio > 0 ? Math.max(segment.ratio, IKStorage.minimumSegmentRatio) : 0;
    }

    clip: true
    color: IKColors.storageSurface
    radius: IKRadius.r12
    implicitHeight: header.height + legend.implicitHeight

    Item {
        id: header

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: IKStorage.headerHeight

        Text {
            anchors.left: parent.left
            anchors.leftMargin: IKStorage.cardHorizontalPadding
            anchors.right: headerTrailing.left
            anchors.rightMargin: IKSpacing.s16
            y: IKStorage.headerTopPadding
            color: IKColors.textPrimary
            elide: Text.ElideRight
            font.pixelSize: IKFonts.bodySize
            font.weight: IKFonts.medium
            text: root.title
            textFormat: Text.PlainText
            visible: !root.loading
        }

        Row {
            id: headerTrailing

            anchors.right: parent.right
            anchors.rightMargin: IKStorage.cardHorizontalPadding
            y: IKStorage.headerTopPadding
            height: IKFonts.bodySize + IKSpacing.s4
            spacing: IKStorage.headerAccessorySpacing

            Text {
                anchors.verticalCenter: parent.verticalCenter
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.bodySize
                font.weight: IKFonts.medium
                text: root.usageText
                visible: !root.loading && root.usageText.length > 0
            }

            IKLoadingSpinner {
                Accessible.name: qsTrId("storageLoadingHint")
                anchors.verticalCenter: parent.verticalCenter
                height: IKStorage.loadingSpinnerSize
                visible: root.loading
                width: IKStorage.loadingSpinnerSize
            }

            // Hidden without an item so the row adds no spacing for an absent accessory.
            Loader {
                anchors.verticalCenter: parent.verticalCenter
                active: !!root.headerAccessory && !root.loading
                visible: item !== null
                sourceComponent: root.headerAccessory
            }
        }

        Rectangle {
            id: storageBar

            readonly property real displayedRatioTotal: {
                let total = root.availableRatio;
                for (const segment of root.segments) {
                    total += root.displayedRatio(segment);
                }
                return total > 0 ? total : 1;
            }

            anchors.left: parent.left
            anchors.leftMargin: IKStorage.cardHorizontalPadding
            anchors.right: parent.right
            anchors.rightMargin: IKStorage.cardHorizontalPadding
            clip: true
            color: root.loading ? IKColors.surfaceTertiary : IKColors.storageFree
            height: IKStorage.storageBarHeight
            layer.enabled: true
            radius: IKStorage.storageBarRadius
            y: IKStorage.headerTopPadding + IKFonts.bodySize + IKSpacing.s12

            layer.effect: MultiEffect {
                maskEnabled: true
                maskSource: storageBarMask
            }

            Row {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                visible: !root.loading

                Repeater {
                    model: root.segments

                    delegate: Rectangle {
                        id: segmentItem

                        required property StorageSegment modelData
                        required property int index

                        height: storageBar.height
                        width: storageBar.width * root.displayedRatio(modelData) / storageBar.displayedRatioTotal
                        color: modelData.color

                        Behavior on width {
                            enabled: root.animated

                            NumberAnimation {
                                duration: IKStorage.segmentAnimationDuration
                                easing.type: Easing.OutCubic
                            }
                        }

                        // Separates this segment from the next one, or from the free space when there is some.
                        Rectangle {
                            anchors.right: parent.right
                            color: IKColors.storageBarSeparator
                            height: parent.height
                            width: 1
                            visible: segmentItem.width >= 2
                                     && (segmentItem.index < root.segments.length - 1 || root.availableRatio > 0)
                        }
                    }
                }
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
        id: legend

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom

        Repeater {
            model: root.entries

            delegate: LegendRow {
                width: legend.width
                height: IKStorage.rowHeight
                visible: modelData.shown
            }
        }
    }

    component LegendRow: Item {
        id: legendRow

        required property StorageLegendEntry modelData

        readonly property bool hasChip: modelData.chipTitle.length > 0

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            color: IKColors.storageDivider
            height: 1
        }

        Rectangle {
            id: dot

            anchors.left: parent.left
            anchors.leftMargin: IKStorage.cardHorizontalPadding
            anchors.verticalCenter: parent.verticalCenter
            color: legendRow.modelData.dotColor
            height: IKStorage.legendDotSize
            radius: width / 2
            width: IKStorage.legendDotSize
        }

        Row {
            anchors.left: dot.right
            anchors.leftMargin: IKStorage.legendLabelSpacing
            anchors.right: valueItem.left
            anchors.rightMargin: IKSpacing.s16
            anchors.verticalCenter: parent.verticalCenter
            spacing: IKStorage.legendLabelSpacing

            Text {
                id: labelText

                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(implicitWidth,
                                parent.width - (legendRow.hasChip ? chip.width + parent.spacing : 0))
                color: IKColors.textPrimary
                elide: Text.ElideRight
                font.pixelSize: IKFonts.bodySize
                font.weight: IKFonts.medium
                text: legendRow.modelData.label
            }

            StorageSyncChip {
                id: chip

                anchors.verticalCenter: parent.verticalCenter
                visible: legendRow.hasChip
                width: Math.min(implicitWidth, Math.max(0, parent.width - labelText.implicitWidth - parent.spacing))
                title: legendRow.modelData.chipTitle
                driveColor: legendRow.modelData.chipColor
                advancedSync: legendRow.modelData.chipAdvancedSync
            }
        }

        Item {
            id: valueItem

            anchors.right: parent.right
            anchors.rightMargin: IKStorage.cardHorizontalPadding
            anchors.verticalCenter: parent.verticalCenter
            height: IKFonts.bodySize + IKSpacing.s4
            width: legendRow.modelData.valueLoading ? IKStorage.loadingPlaceholderWidth : valueText.implicitWidth

            Rectangle {
                anchors.centerIn: parent
                color: IKColors.surfaceTertiary
                height: IKStorage.loadingPlaceholderHeight
                radius: IKRadius.r4
                visible: legendRow.modelData.valueLoading
                width: IKStorage.loadingPlaceholderWidth
            }

            Text {
                id: valueText

                anchors.centerIn: parent
                color: IKColors.textSecondary
                font.pixelSize: IKFonts.bodySize
                font.weight: IKFonts.medium
                text: legendRow.modelData.value
                visible: !legendRow.modelData.valueLoading
            }
        }
    }
}
