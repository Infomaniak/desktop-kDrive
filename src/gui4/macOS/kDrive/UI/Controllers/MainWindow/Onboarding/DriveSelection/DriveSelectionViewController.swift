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

enum OnboardingLinks {
    static let shopDriveSelection = URL(string: "https://shop.infomaniak.com/order/select/drive")!
    static let myKSuiteOffers = URL(string: "https://www.infomaniak.com/gtl/myksuite#prices")!
}

final class DriveSelectionViewController: OnboardingStepViewController {
    private let viewModel: DriveSelectionViewModel
    private let flowCoordinator: OnboardingFlowCoordinator

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

    private lazy var drivesListView: DrivesListView = {
        let drivesListView = DrivesListView()
        drivesListView.translatesAutoresizingMaskIntoConstraints = false
        drivesListView.toggleDrive = viewModel.toggleDriveSelection
        return drivesListView
    }()

    init(flowCoordinator: OnboardingFlowCoordinator) {
        viewModel = DriveSelectionViewModel(flowCoordinator: flowCoordinator)
        self.flowCoordinator = flowCoordinator
        super.init(nibName: nil, bundle: nil)
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

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
        stackView.setCustomSpacing(AppPadding.padding24, after: labeledUserView)

        stackView.insertArrangedSubview(noDriveAvailableView, at: 2)
        stackView.setCustomSpacing(AppPadding.padding24, after: noDriveAvailableView)
        noDriveAvailableView.isHidden = true

        stackView.insertArrangedSubview(drivesListView, at: 3)
        stackView.setCustomSpacing(AppPadding.padding24, after: drivesListView)
        drivesListView.isHidden = true
    }

    private func bindViewModel() {
        flowCoordinator.$currentUser
            .receiveOnMain(store: &bindStore) { [weak self] user in
                guard let user else { return }
                self?.labeledUserView.user = user
            }

        viewModel.$availableDrives
            .receiveOnMain(store: &bindStore) { [weak self] availableDrives in
                self?.handleUpdatedDrivesList(availableDrives)
            }

        viewModel.$selectedDrives
            .receiveOnMain(store: &bindStore) { [weak self] selectedDrives in
                self?.handleSelectedDrivesChanged(selectedDrives)
            }

        viewModel.$isLoading
            .receiveOnMain(store: &bindStore) { [weak self] isLoading in
                self?.handleLoadingState(isLoading)
            }
    }

    private func handleUpdatedDrivesList(_ drives: [UIAvailableDrive]) {
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
    private func updateDrivesList(_ drives: [UIAvailableDrive]) {
        noDriveAvailableView.isHidden = true
        drivesListView.isHidden = false

        drivesListView.drives = drives

        if drives.count == 1, let singleDrive = drives.first {
            drivesListView.cells[singleDrive.id]?.state = .on
            drivesListView.cells[singleDrive.id]?.isEnabled = false
            viewModel.toggleDriveSelection(singleDrive)
        }

        primaryButton.title = KDriveLocalizable.buttonContinue
        primaryButton.action = #selector(didTapContinue)
        secondaryButton.title = KDriveLocalizable.buttonAdvancedParameters
        secondaryButton.action = #selector(didTapAdvancedSettings)
    }

    @objc private func didTapContinue() {
        viewModel.startSynchronization()
    }

    @objc private func didTapAdvancedSettings() {
        guard let currentUser = flowCoordinator.currentUser else { return }

        let viewController = SynchroConfigurationFlowViewController(
            userDbId: Int(currentUser.dbId),
            configurations: viewModel.selectedSynchroConfigurations,
            onConfirm: handleUpdateConfigurations,
            onCancel: dismissPresentedViewController
        )
        presentAsSheet(viewController)
    }

    private func handleSelectedDrivesChanged(_ selectedDrives: Set<UIAvailableDrive>) {
        let shouldEnableButtons = !selectedDrives.isEmpty

        primaryButton.isEnabled = shouldEnableButtons
        secondaryButton.isEnabled = shouldEnableButtons

        drivesListView.selectedDrives = selectedDrives
    }

    private func handleLoadingState(_ isLoading: Bool) {
        toggleCellsEnabledState(!isLoading)
        if isLoading {
            showLoadingButtonsLabel(withText: KDriveLocalizable.onboardingDriveSelectionSyncInProgress)
        } else {
            hideLoadingButtonsLabel()
        }
    }

    private func toggleCellsEnabledState(_ isEnabled: Bool) {
        for cell in drivesListView.cells.values {
            cell.isEnabled = isEnabled
        }
    }

    private func handleUpdateConfigurations(_ configurations: [SynchroConfiguration]) {
        for configuration in configurations {
            viewModel.synchroConfigurations[configuration.drive.id] = configuration
        }
        dismissPresentedViewController()
    }

    private func dismissPresentedViewController() {
        guard let presentedViewController = presentedViewControllers?.first else {
            return
        }
        dismiss(presentedViewController)
    }
}

// MARK: - No drive available

extension DriveSelectionViewController {
    private func showNoDriveAvailableView() {
        noDriveAvailableView.isHidden = false
        drivesListView.isHidden = true

        primaryButton.title = KDriveLocalizable.buttonStartForFree
        primaryButton.action = #selector(didTapStartForFree)
        secondaryButton.title = KDriveLocalizable.buttonShowOffers
        secondaryButton.action = #selector(didTapShowOffers)
    }

    @objc private func didTapStartForFree() {
        NSWorkspace.shared.open(OnboardingLinks.shopDriveSelection)
    }

    @objc private func didTapShowOffers() {
        NSWorkspace.shared.open(OnboardingLinks.myKSuiteOffers)
    }
}
