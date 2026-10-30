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
import QtQuick

// One legend row of a Storage card. A non-empty chipTitle adds the synchronization or drive chip after the label.
QtObject {
    property color dotColor: "transparent"
    property string label
    property string value
    property bool valueLoading: false
    property bool shown: true
    property string chipTitle
    property color chipColor: "transparent"
    property bool chipAdvancedSync: false
}
