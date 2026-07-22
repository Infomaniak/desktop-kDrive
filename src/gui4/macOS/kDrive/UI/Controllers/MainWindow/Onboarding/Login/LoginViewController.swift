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
import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import kDriveResources

final class LoginViewController: OnboardingStepViewController {
    @LazyInjectService private var matomo: MatomoUtils

    private let viewModel: LoginViewModel

    private var bindStore = Set<AnyCancellable>()

    private let cancelButton: NSButton = {
        let button = NSButton(title: KDriveLocalizable.buttonCancel, target: nil, action: nil)
        button.bezelStyle = .push
        button.translatesAutoresizingMaskIntoConstraints = false
        button.isHidden = true
        return button
    }()

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

        cancelButton.target = self
        cancelButton.action = #selector(cancelLogin)
        view.addSubview(cancelButton)

        NSLayoutConstraint.activate([
            cancelButton.topAnchor.constraint(equalTo: buttonsStack.bottomAnchor, constant: AppPadding.padding16),
            cancelButton.leadingAnchor.constraint(equalTo: stackView.leadingAnchor)
        ])
    }

    @objc private func openLoginWebView() {
        matomo.track(eventWithCategory: .onboardingWelcomePage, name: "openSignInWeb")
        viewModel.startWebAuthenticationLogin()
    }

    @objc private func openCreateAccount() {
        matomo.track(eventWithCategory: .onboardingWelcomePage, name: "openSignUpWeb")
        viewModel.openAccountRegistrationProcess()
    }

    @objc private func cancelLogin() {
        viewModel.cancelWebAuthenticationLogin()
    }

    private func handleStateUpdate(_ newState: LoginViewModel.LoginState) {
        switch newState {
        case .idle:
            hideLoadingButtonsLabel()
            cancelButton.isHidden = true
        case .waitingForWebAuthentication:
            showLoadingButtonsLabel(withText: KDriveLocalizable.onboardingLoginHintWebAuth)
            cancelButton.isHidden = false
        case .loadingUser:
            showLoadingButtonsLabel(withText: KDriveLocalizable.onboardingLoginHintLoading)
            cancelButton.isHidden = true
        }
    }

    private func showGenericErrorAlert() {
        let alert = NSAlert()
        alert.alertStyle = .critical
        alert.messageText = KDriveLocalizable.onboardingLoginErrorTitle
        alert.informativeText = KDriveLocalizable.onboardingLoginErrorDescription
        alert.runModal()

        matomo.track(eventWithCategory: .onboardingConnectionFailedPage, name: "reOpenLoginWeb")
        viewModel.isShowingError = false
    }
}
