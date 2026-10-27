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

pragma Singleton

import QtQuick

// Translates the texts of the errors page, which ErrorListModel exposes as translation ids.
QtObject {
    // Fills each of the `argCount` placeholders with the translated node label ("file" or "folder"), or the first one
    // with `count` (the conflicts of the aggregated row, the inotify watches limit), grouped like the language does. A text that starts with the node label ("%1 name too long") gets
    // a capital first letter; the others keep theirs, so that "kDrive" keeps its lowercase "k".
    function text(id, argCount, nodeLabelId, count) {
        if (!id) {
            return "";
        }

        const translation = qsTrId(id);
        let result = count > 0 ? translation.arg(count.toLocaleString(Qt.locale(), "f", 0)) : translation;
        const nodeLabel = nodeLabelId ? qsTrId(nodeLabelId) : "";
        for (let index = 0; index < argCount; ++index) {
            result = result.arg(nodeLabel);
        }
        if (argCount > 0 && translation.startsWith("%1") && result.length > 0) {
            result = result.charAt(0).toUpperCase() + result.slice(1);
        }
        return result;
    }
}
