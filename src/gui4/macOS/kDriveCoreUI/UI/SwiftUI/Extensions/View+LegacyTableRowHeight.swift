/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2026 Infomaniak Network SA

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

import AppKit
import SwiftUI

public extension View {
    /// Gives the `NSTableView` backing a SwiftUI `Table` a fixed row height on macOS 12.
    ///
    /// On macOS 12, `Table` rows keep the default fixed height, so multi-line cells overflow into the next row.
    /// SwiftUI offers no row height API there, so this looks up the enclosing `NSTableView` from a cell.
    /// Apply it to the content of one column cell. It does nothing on macOS 13 and later, or if no table is found.
    @ViewBuilder
    func legacyTableRowHeight(_ rowHeight: CGFloat) -> some View {
        if #available(macOS 13.0, *) {
            self
        } else {
            background(TableRowHeightSetter(rowHeight: rowHeight))
        }
    }
}

private struct TableRowHeightSetter: NSViewRepresentable {
    let rowHeight: CGFloat

    func makeNSView(context: Context) -> TableRowHeightSetterView {
        return TableRowHeightSetterView(rowHeight: rowHeight)
    }

    func updateNSView(_ nsView: TableRowHeightSetterView, context: Context) {
        nsView.rowHeight = rowHeight
        nsView.applyRowHeight()
    }
}

private final class TableRowHeightSetterView: NSView {
    var rowHeight: CGFloat

    init(rowHeight: CGFloat) {
        self.rowHeight = rowHeight
        super.init(frame: .zero)
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        applyRowHeight()
    }

    override func hitTest(_ point: NSPoint) -> NSView? {
        // Let clicks reach the row so selection keeps working.
        return nil
    }

    func applyRowHeight() {
        guard window != nil else { return }

        var ancestor = superview
        while let view = ancestor, !(view is NSTableView) {
            ancestor = view.superview
        }

        guard let tableView = ancestor as? NSTableView, tableView.rowHeight != rowHeight else { return }
        tableView.rowHeight = rowHeight
        // Rows already laid out keep their cached height until the table is told it changed.
        tableView.noteHeightOfRows(withIndexesChanged: IndexSet(integersIn: 0 ..< tableView.numberOfRows))
    }
}
