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
import kDriveCoreUI
import kDriveResources

final class LoginViewController: OnboardingStepViewController {
    private let loginViewModel: LoginViewModel

    private var bindStore = Set<AnyCancellable>()

    init(flowCoordinator: OnboardingFlowCoordinator) {
        loginViewModel = LoginViewModel(flowCoordinator: flowCoordinator)
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
        markButtonsAsLoading(loginViewModel.isShowingAuthenticationWindow)
        loginViewModel.$isShowingAuthenticationWindow
            .receive(on: RunLoop.main)
            .sink { [weak self] isShowing in
                self?.markButtonsAsLoading(isShowing)
            }
            .store(in: &bindStore)

        loginViewModel.$isShowingError
            .receive(on: RunLoop.main)
            .sink { [weak self] isShowingError in
                guard isShowingError else { return }
                self?.showGenericErrorAlert()
            }
            .store(in: &bindStore)
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

    private func markButtonsAsLoading(_ isLoading: Bool) {
        primaryButton.isEnabled = !isLoading
        secondaryButton.isEnabled = !isLoading
    }

    private func showGenericErrorAlert() {
        let alert = NSAlert()
        alert.alertStyle = .critical
        alert.messageText = KDriveLocalizable.onboardingErrorTitle
        alert.informativeText = KDriveLocalizable.onboardingLoginErrorDescription
        alert.runModal()

        loginViewModel.isShowingError = false
    }

    @objc private func openLoginWebView() {
        loginViewModel.startWebAuthenticationLogin(anchor: view.window)
    }

    @objc private func openCreateAccount() {
        // TODO: Handle account creation
    }
}
