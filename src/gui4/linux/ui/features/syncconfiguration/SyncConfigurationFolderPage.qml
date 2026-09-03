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

Column {
    id: root

    required property var controller

    width: parent ? parent.width : implicitWidth
    spacing: IKSyncConfiguration.sectionSpacing

    // The tree is the only control of this page, so it takes the focus as soon as the page is loaded.
    Component.onCompleted: Qt.callLater(function() {
        folderTree.keyboardFocusItem.forceActiveFocus()
    })

    Text {
        width: parent.width
        text: qsTrId("selectFoldersToSyncDescription")
        color: IKColors.textSecondary
        font.pixelSize: IKFonts.bodySize
        wrapMode: Text.WordWrap
    }

    RemoteFolderTree {
        id: folderTree

        width: parent.width
        treeModel: root.controller.folderTreeModel
    }
}
