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

        primaryButton.title = KDriveLocalizable.buttonContinue
        primaryButton.target = self
        primaryButton.action = #selector(didTapContinue)
        secondaryButton.title = KDriveLocalizable.buttonAdvancedParameters
        secondaryButton.target = self
        secondaryButton.action = #selector(didTapAdvancedSettings)

        stackView.insertArrangedSubview(labeledUserView, at: 1)
        stackView.setCustomSpacing(AppPadding.padding12, after: titleLabel)
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
                self?.updateDrivesList(availableDrives)
            }
    }

    private func updateDrivesList(_ drives: [UIDrive]) {
        guard !drives.isEmpty else {
            showNoDriveAvailableView()
            return
        }

        // TODO: List drives
    }

    private func showNoDriveAvailableView() {
        // TODO: Show no kDrive available view
    }

    @objc private func didTapContinue() {
        // dummy implementation
        @InjectService var windowRouter: WindowRouter
        windowRouter.navigate(to: .splitView)
    }

    @objc private func didTapAdvancedSettings() {}
}
