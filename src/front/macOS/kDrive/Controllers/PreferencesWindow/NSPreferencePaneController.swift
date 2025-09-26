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
import kDriveCoreUI

open class NSPreferencePaneController: TitledViewController {
    var scrollView: NSScrollView!
    var stackView: NSStackView!

    open override func viewDidLoad() {
        super.viewDidLoad()
        setupStackViewWithinScrollView()
    }

    private func setupStackViewWithinScrollView() {
        scrollView = NSScrollView()
        scrollView.translatesAutoresizingMaskIntoConstraints = false
        scrollView.hasVerticalScroller = true
        scrollView.autohidesScrollers = true
        scrollView.drawsBackground = false
        scrollView.contentView = FlippedClipView()
        view.addSubview(scrollView)

        stackView = NSStackView()
        stackView.translatesAutoresizingMaskIntoConstraints = false
        stackView.orientation = .vertical
        stackView.edgeInsets = NSEdgeInsets(top: 0, left: 16, bottom: 16, right: 16)
        scrollView.documentView = stackView

        NSLayoutConstraint.activate([
            scrollView.topAnchor.constraint(equalTo: view.topAnchor),
            scrollView.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor),
            scrollView.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor),
            scrollView.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor),

            stackView.topAnchor.constraint(equalTo: scrollView.contentView.topAnchor),
            stackView.leadingAnchor.constraint(equalTo: scrollView.contentView.leadingAnchor),
            stackView.trailingAnchor.constraint(equalTo: scrollView.contentView.trailingAnchor)
        ])
    }

    public func insertNewGroupedSection(with rows: [[NSView]], title: String) {
        let box = createContainerBox(title: title)

        let grid = buildGridWithDividers(rows: rows)

        let gridView = NSGridView(views: rows)
        gridView.translatesAutoresizingMaskIntoConstraints = false
        mergeCellsWithDivider(of: gridView)

        gridView.rowSpacing = 8
        gridView.columnSpacing = 8

        gridView.yPlacement = .center

        gridView.column(at: 0).xPlacement = .leading
        gridView.column(at: 1).xPlacement = .trailing

        box.contentView = gridView

        NSLayoutConstraint.activate([
            gridView.topAnchor.constraint(equalTo: box.topAnchor, constant: 16),
            gridView.leadingAnchor.constraint(equalTo: box.leadingAnchor, constant: 16),
            gridView.trailingAnchor.constraint(equalTo: box.trailingAnchor, constant: -16),
            gridView.bottomAnchor.constraint(equalTo: box.bottomAnchor, constant: -16)
        ])

        stackView.addView(box, in: .bottom)
    }

    private func mergeCellsWithDivider(of grid: NSGridView) {
        let numberOfRows = grid.numberOfRows
        let numberOfColumns = grid.numberOfColumns

        for rowIndex in 0..<numberOfRows {
            let row = grid.row(at: rowIndex)
            if row.numberOfCells == 1, let cell = row.cell(at: 0).contentView as? NSSeparator {
                row.mergeCells(in: NSRange(location: 0, length: numberOfColumns))
            }
        }
    }

    private func buildGridWithDividers(rows: [[NSView]]) -> [[NSView]] {
        var grid = [[NSView]]()
        for row in rows {
            grid.append(row)

            if row != rows.last {
                grid.append([NSSeparator()])
            }
        }

        return grid
    }

    private func createContainerBox(title: String?) -> NSBox {
        let box = NSBox()
        box.translatesAutoresizingMaskIntoConstraints = false
        if let title {
            box.title = title
        } else {
            box.titlePosition = .noTitle
        }

        return box
    }
}
