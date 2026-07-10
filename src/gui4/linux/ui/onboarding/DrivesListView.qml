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
import kDrive.UI

ListView {
    id: root

    required property var drivesModel

    implicitWidth: IKOnboarding.driveSelectionListWidth
    implicitHeight: Math.min(contentHeight, IKOnboarding.driveSelectionListMaxHeight)
    width: IKOnboarding.driveSelectionListWidth
    spacing: IKSpacing.s8
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    model: root.drivesModel

    delegate: DriveCellView {
        required property int index
        required property var model

        width: root.width
        row: index
        driveName: model.name
        accountName: model.accountName
        driveColor: model.color
        checked: model.selected
        cellEnabled: model.enabled
        disabledTooltip: model.tooltip
        onToggled: row => root.drivesModel.toggleDrive(row)
    }
}
