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
import kDrive.UI

// Shared system-resize hit areas for the custom frameless window, including interaction layers placed above a modal.
Item {
    id: root

    required property var targetWindow
    required property real surfaceInset

    readonly property real surfaceWidth: Math.max(0, width - 2 * surfaceInset)
    readonly property real surfaceHeight: Math.max(0, height - 2 * surfaceInset)

    MouseArea {
        x: root.surfaceInset - width
        y: root.surfaceInset
        width: IKWindow.resizeHandleThickness
        height: root.surfaceHeight
        cursorShape: Qt.SizeHorCursor
        onPressed: root.targetWindow.startSystemResize(Qt.LeftEdge)
    }

    MouseArea {
        x: root.surfaceInset + root.surfaceWidth
        y: root.surfaceInset
        width: IKWindow.resizeHandleThickness
        height: root.surfaceHeight
        cursorShape: Qt.SizeHorCursor
        onPressed: root.targetWindow.startSystemResize(Qt.RightEdge)
    }

    MouseArea {
        x: root.surfaceInset
        y: root.surfaceInset - height
        width: root.surfaceWidth
        height: IKWindow.resizeHandleThickness
        cursorShape: Qt.SizeVerCursor
        onPressed: root.targetWindow.startSystemResize(Qt.TopEdge)
    }

    MouseArea {
        x: root.surfaceInset
        y: root.surfaceInset + root.surfaceHeight
        width: root.surfaceWidth
        height: IKWindow.resizeHandleThickness
        cursorShape: Qt.SizeVerCursor
        onPressed: root.targetWindow.startSystemResize(Qt.BottomEdge)
    }

    MouseArea {
        x: root.surfaceInset - width
        y: root.surfaceInset - height
        width: IKWindow.resizeHandleThickness
        height: IKWindow.resizeHandleThickness
        cursorShape: Qt.SizeFDiagCursor
        onPressed: root.targetWindow.startSystemResize(Qt.TopEdge | Qt.LeftEdge)
    }

    MouseArea {
        x: root.surfaceInset + root.surfaceWidth
        y: root.surfaceInset - height
        width: IKWindow.resizeHandleThickness
        height: IKWindow.resizeHandleThickness
        cursorShape: Qt.SizeBDiagCursor
        onPressed: root.targetWindow.startSystemResize(Qt.TopEdge | Qt.RightEdge)
    }

    MouseArea {
        x: root.surfaceInset - width
        y: root.surfaceInset + root.surfaceHeight
        width: IKWindow.resizeHandleThickness
        height: IKWindow.resizeHandleThickness
        cursorShape: Qt.SizeBDiagCursor
        onPressed: root.targetWindow.startSystemResize(Qt.BottomEdge | Qt.LeftEdge)
    }

    MouseArea {
        x: root.surfaceInset + root.surfaceWidth
        y: root.surfaceInset + root.surfaceHeight
        width: IKWindow.resizeHandleThickness
        height: IKWindow.resizeHandleThickness
        cursorShape: Qt.SizeFDiagCursor
        onPressed: root.targetWindow.startSystemResize(Qt.BottomEdge | Qt.RightEdge)
    }
}
