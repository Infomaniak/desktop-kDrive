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
import InfomaniakDI
import kDriveCoreUI
import kDriveResources

class DriveSelectionViewController: OnboardingStepViewController {
    private let viewModel = DriveSelectionViewModel()

    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
    }

    override func viewWillAppear() {
        super.viewWillAppear()
        Task {
            do {
                try await viewModel.loadAvailableDrives()
            } catch {
                print("Error loading available drives: \(error)")
            }
        }
    }

    private func setupUI() {
        titleLabel.stringValue = KDriveLocalizable.onboardingDriveSelectionTitle
        descriptionLabel.isHidden = true

        primaryButton.title = KDriveLocalizable.buttonContinue
        primaryButton.target = self
        primaryButton.action = #selector(didTapContinue)
        secondaryButton.title = KDriveLocalizable.buttonAdvancedParameters
        secondaryButton.target = self
        secondaryButton.action = #selector(didTapAdvancedSettings)
    }

    @objc private func didTapContinue() {
        // dummy implementation
        @InjectService var windowRouter: WindowRouter
        windowRouter.navigate(to: .splitView)
    }

    @objc private func didTapAdvancedSettings() {}
}
