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
        scrollView.contentView = FlippedClipView()
        view.addSubview(scrollView)

        let stackView = NSStackView()
        stackView.translatesAutoresizingMaskIntoConstraints = false
        stackView.orientation = .vertical
        stackView.edgeInsets = NSEdgeInsets(top: 0, left: 16, bottom: 16, right: 16)
        scrollView.documentView = stackView

        setupBox(in: stackView)

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

    private func setupBox(in sourceStackView: NSStackView) {
        let box = NSBox()
        box.translatesAutoresizingMaskIntoConstraints = false
        box.titlePosition = .noTitle
        sourceStackView.addView(box, in: .bottom)

        let automaticUpdateLabel = NSTextField(labelWithString: "Mises à jour automatiques")
        let automaticUpdateSwitch = NSSwitch()

        let languageLabel = NSTextField(labelWithString: "Langue")
        let languagePopUpButton = NSPopUpButton()

        let startAtLoginLabel = NSTextField(labelWithString: "Ouvrir kDrive au démarrage de l’ordinateur")
        let startAtLoginSwitch = NSSwitch()

        let aboutLabel = NSTextField(labelWithString: "À propos de kDrive")
        let aboutButton = NSButton()
        aboutButton.bezelStyle = .helpButton
        aboutButton.controlSize = .small

        let gridView = NSGridView(views: [
            [automaticUpdateLabel, automaticUpdateSwitch],
            [NSSeparator()],
            [languageLabel, languagePopUpButton],
            [NSSeparator()],
            [startAtLoginLabel, startAtLoginSwitch],
            [NSSeparator()],
            [aboutLabel, aboutButton]
        ])
        gridView.translatesAutoresizingMaskIntoConstraints = false

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
    }
}

@available(macOS 14, *)
#Preview {
    GeneralPreferencesViewController()
}
