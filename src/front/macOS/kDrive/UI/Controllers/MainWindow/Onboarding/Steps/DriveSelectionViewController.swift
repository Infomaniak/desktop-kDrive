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

class DriveSelectionViewController: OnboardingStepViewController {
    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
    }

    private func setupUI() {
        titleLabel.stringValue = "!Content de vous retrouver !"
        descriptionLabel.isHidden = true

        primaryButton.stringValue = "!Continuer"
        primaryButton.target = self
        primaryButton.action = #selector(didTapContinue)
        secondaryButton.stringValue = "!Paramètres avancés"
        secondaryButton.target = self
        secondaryButton.action = #selector(didTapAdvancedSettings)
    }

    @objc private func didTapContinue() {

    }

    @objc private func didTapAdvancedSettings() {

    }
}
