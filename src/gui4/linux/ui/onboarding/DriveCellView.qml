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

import QtQuick
import QtQuick.Controls
import kDrive.UI

Item {
    id: root

    property int row: -1
    property string driveName: ""
    property string accountName: ""
    property color driveColor: IKColors.driveDefaultColor
    property bool checked: false
    property bool cellEnabled: true
    property string disabledTooltip: ""

    signal toggled(int row)

    function driveNameContainsMouse() {
        const point = cellMouseArea.mapToItem(driveNameText, cellMouseArea.mouseX, cellMouseArea.mouseY)
        return point.x >= 0 && point.x <= driveNameText.width && point.y >= 0 && point.y <= driveNameText.height
    }

    implicitWidth: IKOnboarding.driveSelectionListWidth
    implicitHeight: Math.max(IKOnboarding.driveSelectionCellMinHeight,
                             contentRow.implicitHeight + IKOnboarding.driveSelectionCellPadding * 2)

    Rectangle {
        anchors.fill: parent
        radius: IKOnboarding.driveSelectionCellRadius
        color: cellMouseArea.containsMouse && root.cellEnabled
               ? IKColors.surfaceSecondary
               : IKColors.onboardingDriveCellSurface
        opacity: root.cellEnabled ? 1 : 0.5
    }

    Row {
        id: contentRow

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: IKOnboarding.driveSelectionCellPadding
        anchors.rightMargin: IKOnboarding.driveSelectionCellPadding
        spacing: IKOnboarding.driveSelectionCellSpacing

        Rectangle {
            width: IKOnboarding.driveSelectionCheckboxSize
            height: IKOnboarding.driveSelectionCheckboxSize
            anchors.verticalCenter: parent.verticalCenter
            radius: IKRadius.r4
            color: root.checked ? IKColors.actionPrimary : "transparent"
            border.width: root.checked ? 0 : 1
            border.color: root.cellEnabled ? IKColors.textTertiary : IKColors.onboardingDriveDisabledText

            Text {
                anchors.centerIn: parent
                visible: root.checked
                text: "✓"
                color: IKColors.actionOnPrimary
                font.pixelSize: IKFonts.subheadlineSize
                font.weight: IKFonts.emphasized
            }
        }

        DriveIconView {
            width: IKOnboarding.driveSelectionDriveIconSize
            height: IKOnboarding.driveSelectionDriveIconSize
            anchors.verticalCenter: parent.verticalCenter
            iconColor: root.cellEnabled ? root.driveColor : IKColors.onboardingDriveDisabledText
        }

        Column {
            width: Math.max(0, contentRow.width - IKOnboarding.driveSelectionCheckboxSize
                            - IKOnboarding.driveSelectionDriveIconSize - contentRow.spacing * 2)
            spacing: 0

            Text {
                id: driveNameText

                width: parent.width
                text: root.driveName
                color: root.cellEnabled ? IKColors.textPrimary : IKColors.onboardingDriveDisabledText
                font.pixelSize: IKFonts.bodySize
                lineHeightMode: Text.FixedHeight
                lineHeight: IKOnboarding.driveSelectionDriveNameLineHeight
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                visible: root.accountName.length > 0
                text: root.accountName
                color: root.cellEnabled ? IKColors.textSecondary : IKColors.onboardingDriveDisabledText
                font.pixelSize: IKFonts.subheadlineSize
                lineHeightMode: Text.FixedHeight
                lineHeight: IKOnboarding.driveSelectionAccountLineHeight
                elide: Text.ElideRight
            }
        }
    }

    MouseArea {
        id: cellMouseArea

        anchors.fill: parent
        hoverEnabled: true
        cursorShape: root.cellEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: {
            if (root.cellEnabled) {
                root.toggled(root.row)
            }
        }
    }

    ToolTip {
        id: longDriveNameTooltip

        visible: root.cellEnabled && cellMouseArea.containsMouse && root.driveNameContainsMouse() && driveNameText.truncated
        delay: IKOnboarding.driveSelectionTooltipDelay
        timeout: -1
        text: root.driveName
        padding: IKOnboarding.driveSelectionTooltipPadding

        contentItem: Text {
            width: Math.min(implicitWidth, IKOnboarding.driveSelectionTooltipMaxWidth)
            text: longDriveNameTooltip.text
            color: IKColors.onboardingTooltipText
            font.pixelSize: IKFonts.bodySize
            lineHeightMode: Text.FixedHeight
            lineHeight: IKOnboarding.driveSelectionDriveNameLineHeight
            wrapMode: Text.WordWrap
        }

        background: Rectangle {
            radius: IKOnboarding.driveSelectionTooltipRadius
            color: IKColors.onboardingTooltipSurface
        }
    }

    ToolTip {
        id: disabledCellTooltip

        visible: !root.cellEnabled && cellMouseArea.containsMouse && root.disabledTooltip.length > 0
        delay: IKOnboarding.driveSelectionTooltipDelay
        timeout: -1
        text: root.disabledTooltip
        padding: IKOnboarding.driveSelectionTooltipPadding

        contentItem: Text {
            width: Math.min(implicitWidth, IKOnboarding.driveSelectionTooltipMaxWidth)
            text: disabledCellTooltip.text
            color: IKColors.onboardingTooltipText
            font.pixelSize: IKFonts.bodySize
            lineHeightMode: Text.FixedHeight
            lineHeight: IKOnboarding.driveSelectionDriveNameLineHeight
            wrapMode: Text.WordWrap
        }

        background: Rectangle {
            radius: IKOnboarding.driveSelectionTooltipRadius
            color: IKColors.onboardingTooltipSurface
        }
    }
}
