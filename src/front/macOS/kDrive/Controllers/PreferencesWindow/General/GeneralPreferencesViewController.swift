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

class GeneralPreferencesViewController: PreferencePaneController {
    convenience init() {
        self.init(toolbarTitle: SidebarItem.general.title)
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        setupSection()
    }

    private func setupSection() {
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

        insertNewGroupedSection(with: [
            [automaticUpdateLabel, automaticUpdateSwitch],
            [languageLabel, languagePopUpButton],
            [startAtLoginLabel, startAtLoginSwitch],
            [aboutLabel, aboutButton]
        ])
    }
}

@available(macOS 14, *)
#Preview {
    GeneralPreferencesViewController()
}
