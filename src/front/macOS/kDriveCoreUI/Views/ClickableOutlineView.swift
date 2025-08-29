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

    private(set) var activatedRowIndexes = IndexSet()

    override func mouseDown(with event: NSEvent) {
        super.mouseDown(with: event)

        guard window?.isKeyWindow == true else { return }
        if let item = detectClickedItem(from: event) {
            (delegate as? ClickableOutlineViewDelegate)?.outlineView(self, didClick: item)
        }
    }

    override func rowView(atRow row: Int, makeIfNecessary: Bool) -> NSTableRowView? {
        let tableRow = super.rowView(atRow: row, makeIfNecessary: makeIfNecessary)
        if let tableRow, makeIfNecessary {
            markRowAsActivated(tableRow, isActivated: activatedRowIndexes.contains(row))
        }

        return tableRow
    }

    func showMenu(_ menu: NSMenu, at item: Any) {
        let itemRow = row(forItem: item)
        guard itemRow != -1 else { return }

        let cellFrame = frameOfCell(atColumn: 0, row: itemRow)

        activatedRowIndexes.insert(itemRow)
        if let rowView = rowView(atRow: itemRow, makeIfNecessary: false) {
            markRowAsActivated(rowView, isActivated: true)
        }

        menu.popUp(positioning: nil, at: NSPoint(x: cellFrame.minX, y: cellFrame.maxY + Self.menuTopPadding), in: self)

        activatedRowIndexes.remove(itemRow)
        if let rowView = rowView(atRow: itemRow, makeIfNecessary: false) {
            markRowAsActivated(rowView, isActivated: false)
        }
    }

    private func markRowAsActivated(_ row: NSTableRowView, isActivated: Bool) {
        row.alphaValue = isActivated ? 0.5 : 1.0
    }

    private func detectClickedItem(from event: NSEvent) -> Any? {
        let locationInView = convert(event.locationInWindow, from: nil)
        let targetRow = row(at: locationInView)

        return item(atRow: targetRow)
    }
}
