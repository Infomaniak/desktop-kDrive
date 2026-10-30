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

// Search field of the search dialog, bound to the query of its controller.
IKSearchField {
    id: root

    required property var controller

    signal moveToResultsRequested

    placeholderText: qsTrId("searchBoxPlaceholder").arg(root.controller.driveName)

    // Typing breaks a declarative text binding, so the controller query is pushed back explicitly.
    Component.onCompleted: text = root.controller.query
    onTextEdited: editedText => root.controller.query = editedText
    onCleared: root.controller.query = ""
    onDownPressed: root.moveToResultsRequested()

    Connections {
        target: root.controller

        function onQueryChanged() {
            if (root.text !== root.controller.query) {
                root.text = root.controller.query;
            }
        }
    }
}
