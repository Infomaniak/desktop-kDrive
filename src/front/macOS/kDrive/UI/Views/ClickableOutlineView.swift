/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2025 Infomaniak Network SA

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import Cocoa

protocol ClickableOutlineViewDelegate: NSOutlineViewDelegate {
    func outlineView(_ outlineView: NSOutlineView, didClick item: Any?)
}

final class ClickableOutlineView: NSOutlineView {
    private static let menuTopPadding: CGFloat = 4.0

    override func mouseDown(with event: NSEvent) {
        super.mouseDown(with: event)

        guard window?.isKeyWindow == true else { return }
        if let item = detectClickedItem(from: event) {
            (delegate as? ClickableOutlineViewDelegate)?.outlineView(self, didClick: item)
        }
    }

    func showMenu(_ menu: NSMenu, at item: Any) {
        let itemRow = row(forItem: item)
        guard itemRow != -1 else { return }
        let itemRect = frameOfCell(atColumn: 0, row: itemRow)

        print("Present Menu")
        menu.popUp(positioning: nil, at: NSPoint(x: itemRect.minX, y: itemRect.maxY + Self.menuTopPadding), in: self)
        print("Did present Menu")
    }

    private func detectClickedItem(from event: NSEvent) -> Any? {
        let locationInView = convert(event.locationInWindow, from: nil)
        let targetRow = row(at: locationInView)

        return item(atRow: targetRow)
    }
}
