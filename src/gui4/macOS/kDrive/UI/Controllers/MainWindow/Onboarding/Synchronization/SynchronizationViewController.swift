/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2026 Infomaniak Network SA

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
import kDriveCore
import kDriveResources

final class SynchronizationViewController: OnboardingStepViewController {
    private let viewModel: SynchronizationViewModel

    private var bindStore = Set<AnyCancellable>()

    init(flowCoordinator: OnboardingFlowCoordinator) {
        viewModel = SynchronizationViewModel(flowCoordinator: flowCoordinator)
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
        viewModel.createSynchronizations()
    }

    private func setupUI() {
        titleLabel.stringValue = KDriveLocalizable.onboardingSynchronizationInProgressTitle
        descriptionLabel.stringValue = KDriveLocalizable.onboardingSynchronizationInProgressDescription

        primaryButton.isHidden = true
        secondaryButton.isHidden = true
    }

    private func bindViewModel() {
        viewModel.$isShowingError
            .receiveOnMain(store: &bindStore) { [weak self] isShowingError in
                guard isShowingError else { return }
                self?.showGenericErrorAlert()
            }
    }

    private func showGenericErrorAlert() {
        let alert = NSAlert()
        alert.alertStyle = .critical
        alert.messageText = KDriveLocalizable.unexpectedErrorTeachingTipTitle
        alert.informativeText = KDriveLocalizable.unexpectedErrorTeachingTipContent
        alert.runModal()

        viewModel.isShowingError = false
    }
}
