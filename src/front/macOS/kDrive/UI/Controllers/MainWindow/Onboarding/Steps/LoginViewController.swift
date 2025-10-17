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
    private let viewModel: OnboardingViewModel
    private var bindStore = Set<AnyCancellable>()

    init(viewModel: OnboardingViewModel) {
        self.viewModel = viewModel
        super.init(nibName: nil, bundle: nil)
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        bindViewModel()
    }

    private func bindViewModel() {
        transitionLogin(toStep: viewModel.currentStep)
        viewModel.$currentStep.receive(on: DispatchQueue.main).sink { [weak self] step in
            self?.transitionLogin(toStep: step)
        }
        .store(in: &bindStore)

        markButtonsAsLoading(viewModel.isShowingAuthenticationWindow)
        viewModel.$isShowingAuthenticationWindow.receive(on: DispatchQueue.main).sink { [weak self] isShowing in
            self?.markButtonsAsLoading(isShowing)
        }
        .store(in: &bindStore)
    }

    private func transitionLogin(toStep step: OnboardingStep) {
        guard case .login(let loginStep) = viewModel.currentStep else { return }

        switch loginStep {
        case .initial:
            setupInitialView()
        case .success:
            // TODO: Check if this step is necessary once the server is implemented
            fatalError("Not Implemented Yet")
        case .fail:
            setupErrorView()
        }
    }

    private func setupInitialView() {
        titleLabel.stringValue = KDriveLocalizable.onboardingLoginTitle
        descriptionLabel.stringValue = KDriveLocalizable.onboardingLoginDescription
        setupButtons()
    }

    private func setupErrorView() {
        titleLabel.stringValue = KDriveLocalizable.onboardingErrorTitle
        descriptionLabel.stringValue = KDriveLocalizable.onboardingLoginErrorDescription
        setupButtons()
    }

    private func setupButtons() {
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

    @objc private func openLoginWebView() {
        viewModel.startWebAuthenticationLogin(anchor: view.window)
    }

    @objc private func openCreateAccount() {

    }
}
