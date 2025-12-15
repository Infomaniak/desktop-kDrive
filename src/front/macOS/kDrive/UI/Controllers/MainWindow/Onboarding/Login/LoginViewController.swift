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
import kDriveCore
import kDriveCoreUI
import kDriveResources

import InfomaniakDI

final class LoginViewController: OnboardingStepViewController {
    private let viewModel: LoginViewModel

    private var bindStore = Set<AnyCancellable>()

    init(flowCoordinator: OnboardingFlowCoordinator) {
        viewModel = LoginViewModel(flowCoordinator: flowCoordinator)
        super.init(nibName: nil, bundle: nil)
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func viewDidLoad() {
        super.viewDidLoad()

        bindViewModel()
        setupUI()
    }

    private func bindViewModel() {
        viewModel.$loginState
            .receiveOnMain(store: &bindStore) { [weak self] newState in
                self?.handleStateUpdate(newState)
            }

        viewModel.$isShowingError
            .receiveOnMain(store: &bindStore) { [weak self] isShowingError in
                guard isShowingError else { return }
                self?.showGenericErrorAlert()
            }
    }

    private func setupUI() {
        titleLabel.stringValue = KDriveLocalizable.onboardingLoginTitle
        descriptionLabel.stringValue = KDriveLocalizable.onboardingLoginDescription

        primaryButton.title = KDriveLocalizable.buttonLogin
        primaryButton.target = self
        primaryButton.action = #selector(openLoginWebView)
        secondaryButton.title = KDriveLocalizable.buttonCreateAccount
        secondaryButton.target = self
        secondaryButton.action = #selector(openCreateAccount)
    }

    @objc private func openLoginWebView() {
        viewModel.startWebAuthenticationLogin(anchor: view.window)
    }

    @objc private func openCreateAccount() {
        viewModel.openAccountRegistrationProcess()
    }

    private func handleStateUpdate(_ newState: LoginViewModel.LoginState) {
        switch newState {
        case .idle:
            hideLoadingButtonsLabel()
        case .waitingForWebAuthentication:
            showLoadingButtonsLabel(withText: KDriveLocalizable.onboardingLoginHintWebAuth)
        case .loadingUser:
            showLoadingButtonsLabel(withText: KDriveLocalizable.onboardingLoginHintLoading)
        }
    }

    private func showGenericErrorAlert() {
        let alert = NSAlert()
        alert.alertStyle = .critical
        alert.messageText = KDriveLocalizable.onboardingLoginErrorTitle
        alert.informativeText = KDriveLocalizable.onboardingLoginErrorDescription
        alert.runModal()

        viewModel.isShowingError = false
    }
}
