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
import QtQuick.Shapes
import kDrive.UI

// A drive is picked with the keyboard as well as with the mouse, so the cell is a control rather than a decorated
// mouse target: AbstractButton then handles Space, the focus on click, and the focus ring below through `visualFocus`.
// It stays non-checkable on purpose, because the checked state belongs to the model and a click would overwrite it.
AbstractButton {
    id: root

    property int row: -1
    property string driveName: ""
    property string accountName: ""
    property color driveColor: IKColors.driveDefaultColor
    property bool cellEnabled: true
    property string disabledTooltip: ""

    // Named apart from AbstractButton's own `toggled()`, which a same-named signal would invalidly override.
    signal toggleRequested(int row)

    function requestToggle(): void {
        if (root.cellEnabled) {
            root.toggleRequested(root.row)
        }
    }

    function driveNameContainsMouse() {
        const point = root.mapToItem(driveNameText, cellHover.point.position.x, cellHover.point.position.y)
        return point.x >= 0 && point.x <= driveNameText.width && point.y >= 0 && point.y <= driveNameText.height
    }

    implicitWidth: IKOnboarding.driveSelectionListWidth
    implicitHeight: Math.max(IKOnboarding.driveSelectionCellMinHeight,
                             contentRow.implicitHeight + IKOnboarding.driveSelectionCellPadding * 2)

    focusPolicy: root.cellEnabled ? Qt.StrongFocus : Qt.NoFocus
    onClicked: root.requestToggle()

    Accessible.role: Accessible.CheckBox
    Accessible.name: root.driveName
    Accessible.description: root.accountName
    Accessible.checkable: root.cellEnabled
    Accessible.checked: root.checked
    // Assistive technologies invoke either action depending on how they read the cell, so both reach the model.
    Accessible.onPressAction: root.requestToggle()
    Accessible.onToggleAction: root.requestToggle()

    HoverHandler {
        id: cellHover

        cursorShape: root.cellEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    Rectangle {
        anchors.fill: parent
        radius: IKOnboarding.driveSelectionCellRadius
        color: cellHover.hovered && root.cellEnabled
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

            Shape {
                anchors.fill: parent
                visible: root.checked
                preferredRendererType: Shape.CurveRenderer

                ShapePath {
                    strokeColor: IKColors.actionOnPrimary
                    strokeWidth: 1.5
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    startX: 3.5
                    startY: 8

                    PathLine { x: 6.5; y: 11 }
                    PathLine { x: 12.5; y: 4.5 }
                }
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

    // Drawn above the cell content so the focus ring stays visible over the hover and selection fills. It follows
    // `visualFocus`, so it appears for keyboard navigation only and stays hidden when the cell is clicked.
    Rectangle {
        anchors.fill: parent
        visible: root.visualFocus
        radius: IKOnboarding.driveSelectionCellRadius
        color: "transparent"
        border.width: IKOnboarding.driveSelectionCellFocusBorderWidth
        border.color: IKColors.accentPrimary
    }

    IKToolTip {
        visible: root.cellEnabled && cellHover.hovered && root.driveNameContainsMouse() && driveNameText.truncated
        delay: IKOnboarding.driveSelectionTooltipDelay
        text: root.driveName
        padding: IKOnboarding.driveSelectionTooltipPadding
        maximumTextWidth: IKOnboarding.driveSelectionTooltipMaxWidth
        foregroundColor: IKColors.onboardingTooltipText
        surfaceColor: IKColors.onboardingTooltipSurface
        textLineHeight: IKOnboarding.driveSelectionDriveNameLineHeight
    }

    IKToolTip {
        visible: !root.cellEnabled && cellHover.hovered && root.disabledTooltip.length > 0
        delay: IKOnboarding.driveSelectionTooltipDelay
        text: root.disabledTooltip
        padding: IKOnboarding.driveSelectionTooltipPadding
        maximumTextWidth: IKOnboarding.driveSelectionTooltipMaxWidth
        foregroundColor: IKColors.onboardingTooltipText
        surfaceColor: IKColors.onboardingTooltipSurface
        textLineHeight: IKOnboarding.driveSelectionDriveNameLineHeight
    }
}
