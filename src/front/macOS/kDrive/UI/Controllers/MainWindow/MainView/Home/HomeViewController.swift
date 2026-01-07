/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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

import Cocoa
import kDriveCoreUI

final class HomeViewController: TitledViewController {
    private lazy var greetingTextField: NSTextField = {
        let textField =
            NSTextField(labelWithAttributedString: NSAttributedString(string: "Bonjour Florent, vos fichiers sont à jour"))
        textField.translatesAutoresizingMaskIntoConstraints = false
        return textField
    }()

    private lazy var synchroStatePanel: SynchroStatePanelView = {
        let panel = SynchroStatePanelView()
        panel.translatesAutoresizingMaskIntoConstraints = false
        return panel
    }()

    private lazy var selectedUserAndDrivePanel: SelectedUserAndDrivePanelView = {
        let panel = SelectedUserAndDrivePanelView()
        panel.translatesAutoresizingMaskIntoConstraints = false
        panel.user = mainViewModel.currentUser
        panel.drive = mainViewModel.currentDrive
        return panel
    }()

    private var mainViewModel: MainViewModel

    init(mainViewModel: MainViewModel) {
        self.mainViewModel = mainViewModel
        super.init(toolbarTitle: SidebarItem.home.title)
        setupView()
    }

    private func setupView() {
        view.addSubview(greetingTextField)

        let panelsContainer = NSView()
        panelsContainer.translatesAutoresizingMaskIntoConstraints = false
        panelsContainer.addSubview(synchroStatePanel)
        panelsContainer.addSubview(selectedUserAndDrivePanel)
        view.addSubview(panelsContainer)

        NSLayoutConstraint.activate([
            greetingTextField.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: AppPadding.padding32),
            greetingTextField.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: AppPadding.padding24),
            greetingTextField.trailingAnchor.constraint(lessThanOrEqualTo: view.trailingAnchor, constant: -AppPadding.padding24),

            panelsContainer.topAnchor.constraint(equalTo: greetingTextField.bottomAnchor, constant: AppPadding.padding32),
            panelsContainer.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: AppPadding.padding24),
            panelsContainer.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -AppPadding.padding24),
            panelsContainer.bottomAnchor.constraint(equalTo: view.bottomAnchor, constant: -AppPadding.padding24),

            synchroStatePanel.heightAnchor.constraint(equalTo: panelsContainer.heightAnchor),
            synchroStatePanel.widthAnchor.constraint(equalTo: panelsContainer.widthAnchor, multiplier: 2.0 / 3.0),
            synchroStatePanel.leadingAnchor.constraint(equalTo: panelsContainer.leadingAnchor),
            synchroStatePanel.trailingAnchor.constraint(equalTo: selectedUserAndDrivePanel.leadingAnchor),

            selectedUserAndDrivePanel.heightAnchor.constraint(equalTo: panelsContainer.heightAnchor),
            selectedUserAndDrivePanel.trailingAnchor.constraint(equalTo: panelsContainer.trailingAnchor)
        ])
    }
}
