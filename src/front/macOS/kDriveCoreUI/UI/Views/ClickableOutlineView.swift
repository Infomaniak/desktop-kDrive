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

public protocol ClickableItem: Sendable {
    var isClickable: Bool { get }
}

public protocol ClickableOutlineViewDelegate: NSOutlineViewDelegate {
    func outlineView(_ outlineView: NSOutlineView, didClick item: Any?)
    func outlineView(_ outlineView: NSOutlineView, canClick item: Any?) -> Bool
}

public extension ClickableOutlineViewDelegate {
    func outlineView(_ outlineView: NSOutlineView, canClick item: Any?) -> Bool {
        return true
    }
}

public final class ClickableOutlineView: NSOutlineView {
    override public func mouseDown(with event: NSEvent) {
        handleClickItem(from: event)

        super.mouseDown(with: event)

        toggleRow(from: event, toState: false)
    }

    private func handleClickItem(from event: NSEvent) {
        guard window?.isKeyWindow == true,
              let item = detectClickedItem(from: event), item.isClickable,
              (delegate as? ClickableOutlineViewDelegate)?.outlineView(self, canClick: item) != false
        else { return }

        toggleRow(from: event, toState: true)
        (delegate as? ClickableOutlineViewDelegate)?.outlineView(self, didClick: item)
    }

    private func toggleRow(from event: NSEvent, toState shouldEnable: Bool) {
        guard let item = detectClickedItem(from: event), item.isClickable else {
            return
        }

        guard let row = rowView(for: item) else { return }
        markRowAsActivated(row.view, isActivated: shouldEnable)
    }

    private func rowView(for item: Any) -> (index: Int, view: NSTableRowView)? {
        let itemRow = row(forItem: item)
        guard itemRow != -1 else {
            return nil
        }

        let rowView = rowView(atRow: itemRow, makeIfNecessary: false)
        guard let rowView else {
            return nil
        }

        return (itemRow, rowView)
    }

    private func markRowAsActivated(_ row: NSTableRowView, isActivated: Bool) {
        row.alphaValue = isActivated ? 0.5 : 1.0
    }

    private func detectClickedItem(from event: NSEvent) -> ClickableItem? {
        let locationInView = convert(event.locationInWindow, from: nil)
        let targetRow = row(at: locationInView)

        return item(atRow: targetRow) as? ClickableItem
    }
}
