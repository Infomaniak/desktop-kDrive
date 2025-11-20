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
import Combine
import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import kDriveResources

class DriveSelectionViewController: OnboardingStepViewController {
    private let viewModel = DriveSelectionViewModel()

    private var bindStore = Set<AnyCancellable>()

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

    private func bindViewModel() {
        viewModel.$currentUser
            .receiveOnMain(store: &bindStore) { [weak self] user in
                guard let user else { return }

            }

        viewModel.$availableDrives
            .receiveOnMain(store: &bindStore) { [weak self] availableDrives in
                guard let availableDrives else { return }
                self?.updateDrivesList(availableDrives)
            }
    }

    private func setupCurrentUser(_ user: UIUser) {
        // TODO: Show user info
    }

    private func updateDrivesList(_ drives: [UIDrive]) {
        guard !drives.isEmpty else {
            setupNoKDriveView()
            return
        }

        // TODO: List drives
    }

    private func setupNoKDriveView() {
    }


    @objc private func didTapContinue() {
        // dummy implementation
        @InjectService var windowRouter: WindowRouter
        windowRouter.navigate(to: .splitView)
    }

    @objc private func didTapAdvancedSettings() {}
}
