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

    private lazy var labeledUserView: LabeledAvatarView = {
        let labeledAvatarView = LabeledAvatarView()
        labeledAvatarView.translatesAutoresizingMaskIntoConstraints = false
        return labeledAvatarView
    }()

    private lazy var noDriveAvailableView: NoDriveAvailableView = {
        let noDriveAvailable = NoDriveAvailableView()
        noDriveAvailable.translatesAutoresizingMaskIntoConstraints = false
        return noDriveAvailable
    }()

    override func viewDidLoad() {
        super.viewDidLoad()

        setupUI()
        bindViewModel()
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

        buttonsStack.isHidden = true
        primaryButton.target = self
        secondaryButton.target = self

        stackView.insertArrangedSubview(labeledUserView, at: 1)
        stackView.setCustomSpacing(AppPadding.padding12, after: titleLabel)

        stackView.insertArrangedSubview(noDriveAvailableView, at: 2)
        stackView.setCustomSpacing(AppPadding.padding24, after: titleLabel)
        noDriveAvailableView.isHidden = true
    }

    private func bindViewModel() {
        viewModel.$currentUser
            .receiveOnMain(store: &bindStore) { [weak self] user in
                guard let user else { return }
                self?.labeledUserView.user = user
            }

        viewModel.$availableDrives
            .receiveOnMain(store: &bindStore) { [weak self] availableDrives in
                guard let availableDrives else { return }
                self?.handleUpdatedDrivesList(availableDrives)
            }
    }

    private func handleUpdatedDrivesList(_ drives: [UIDrive]) {
        buttonsStack.isHidden = false

        if drives.isEmpty {
            showNoDriveAvailableView()
        } else {
            updateDrivesList(drives)
        }
    }
}

// MARK: - Update drives list

extension DriveSelectionViewController {
    private func updateDrivesList(_ drives: [UIDrive]) {
        // TODO: List drives

        primaryButton.title = KDriveLocalizable.buttonContinue
        primaryButton.action = #selector(didTapContinue)
        secondaryButton.title = KDriveLocalizable.buttonAdvancedParameters
        secondaryButton.action = #selector(didTapAdvancedSettings)
    }

    @objc private func didTapContinue() {}

    @objc private func didTapAdvancedSettings() {}
}

// MARK: - No drive available

extension DriveSelectionViewController {
    private func showNoDriveAvailableView() {
        // TODO: Show no kDrive available view

        primaryButton.title = "!Commencer gratuitement"
        primaryButton.action = #selector(didTapStartForFree)
        secondaryButton.title = "!Voir les offres"
        secondaryButton.action = #selector(didTapShowOffers)
    }

    @objc private func didTapStartForFree() {}

    @objc private func didTapShowOffers() {}
}
