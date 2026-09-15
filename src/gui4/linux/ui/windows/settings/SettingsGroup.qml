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

Rectangle {
    id: root
    default property alias rows: column.data
    implicitHeight: column.implicitHeight
    radius: IKRadius.r12
    color: IKColors.settingsCardSurface
    Column {
        id: column
        x: IKSettings.groupPadding
        width: root.width - 2 * IKSettings.groupPadding
    }
}
