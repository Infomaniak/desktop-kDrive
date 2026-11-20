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

ListView {
    id: root

    required property var drivesModel
    // Height the list may take; beyond it, the list scrolls.
    property real maximumHeight: contentHeight
    // Distance between two cards, which all have the same height (`driveSelectionCellMinHeight`).
    readonly property real cellPitch: count > 0 ? (contentHeight + spacing) / count : 0

    // An overflowing list stops in the middle of a card, so that the cut card shows that the list scrolls.
    function fittedHeight(): real {
        if (contentHeight <= maximumHeight || cellPitch <= 0) {
            return contentHeight;
        }

        const halfCell = (cellPitch - spacing) / 2;
        const fullCells = Math.max(0, Math.floor((maximumHeight - halfCell) / cellPitch));
        return fullCells * cellPitch + halfCell;
    }

    implicitWidth: IKOnboarding.driveSelectionListWidth + IKOnboarding.driveSelectionListScrollBarGutter
    implicitHeight: fittedHeight()
    height: implicitHeight
    spacing: IKSpacing.s8
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    acceptedButtons: Qt.NoButton // without this, a click after a scroll is taken to stop the scroll and is not forwarded to the button

    model: root.drivesModel

    // In the gutter on the right of the cards, never over them.
    ScrollBar.vertical: ScrollBar {
        policy: root.contentHeight > root.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
    }

    delegate: DriveCellView {
        required property int index
        required property var model

        width: root.width - IKOnboarding.driveSelectionListScrollBarGutter
        row: index
        driveName: model.name
        // The account is shown only when it tells something the drive name does not.
        accountName: model.accountName !== model.name ? model.accountName : ""
        driveColor: model.color
        checked: model.selected
        cellEnabled: model.enabled
        disabledTooltip: model.tooltip
        onToggleRequested: row => root.drivesModel.toggleDrive(row)
        // Tabbing to a cell that sits below the visible part of the list has to bring it into view.
        onActiveFocusChanged: {
            if (activeFocus) {
                root.positionViewAtIndex(index, ListView.Contain)
            }
        }
    }
}
