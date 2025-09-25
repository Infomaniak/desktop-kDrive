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

class GeneralPreferencesViewController: TitledViewController {
    convenience init() {
        self.init(toolbarTitle: SidebarItem.general.title)
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
    }

    private func setupUI() {
        let scrollView = NSScrollView()
        scrollView.translatesAutoresizingMaskIntoConstraints = false
        scrollView.hasVerticalScroller = true
        scrollView.autohidesScrollers = true
        scrollView.drawsBackground = false
        view.addSubview(scrollView)

        let stackView = NSStackView()
        stackView.translatesAutoresizingMaskIntoConstraints = false
        stackView.orientation = .vertical

        let flippedView = FlippedView(contentView: stackView)
        flippedView.translatesAutoresizingMaskIntoConstraints = false
        scrollView.documentView = flippedView

        setupBox(in: stackView)

        NSLayoutConstraint.activate([
            scrollView.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            scrollView.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor),
            scrollView.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor),
            scrollView.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor),

            flippedView.topAnchor.constraint(equalTo: scrollView.contentView.topAnchor),
            flippedView.leadingAnchor.constraint(equalTo: scrollView.contentView.leadingAnchor),
            flippedView.trailingAnchor.constraint(equalTo: scrollView.contentView.trailingAnchor)
        ])
    }

    private func setupBox(in sourceStackView: NSStackView) {
        let box = NSBox()
        box.translatesAutoresizingMaskIntoConstraints = false
//        box.titlePosition = .noTitle
        box.boxType = .primary

        let gridView = NSGridView(numberOfColumns: 2, rows: 6)
        gridView.translatesAutoresizingMaskIntoConstraints = false
//        box.contentView = gridView

        gridView.addRow(with: [
            NSTextField(labelWithString: "Coucou"),
            NSTextField(labelWithString: "Tout le monde")
        ])

        sourceStackView.addView(gridView, in: .bottom)
    }
}

@available(macOS 14, *)
#Preview {
    GeneralPreferencesViewController()
}
