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
import InfomaniakLogin
import kDriveCoreUI
import kDriveLogin
import kDriveResources
import AuthenticationServices

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
        viewModel.$currentStep.receive(on: DispatchQueue.main).sink { [weak self] step in
            self?.transitionContent(toStep: step)
        }
        .store(in: &bindStore)

        viewModel.$isShowingAuthenticationWindow.receive(on: DispatchQueue.main).sink { [weak self] isShowing in
            self?.markButtonsAsLoading(isShowing)
        }
        .store(in: &bindStore)
    }

    private func transitionContent(toStep step: OnboardingStep) {
        guard case .login(let loginStep) = viewModel.currentStep else { return }

        switch loginStep {
        case .initial:
            setupInitialView()
        case .success:
            fatalError("Not Implemented Yet")
        case .fail:
            fatalError("Not Implemented Yet")
        }
    }

    private func setupInitialView() {
        titleLabel.stringValue = KDriveLocalizable.onboardingLoginTitle
        descriptionLabel.stringValue = KDriveLocalizable.onboardingLoginDescription

        primaryButton.title = KDriveLocalizable.buttonLogin
        primaryButton.target = self
        primaryButton.action = #selector(openLoginWebView)
        secondaryButton.title = KDriveLocalizable.buttonCreateAccount
    }

    private func markButtonsAsLoading(_ isLoading: Bool) {
        primaryButton.isEnabled = !isLoading
        secondaryButton.isEnabled = !isLoading
    }

    @objc private func openLoginWebView() {
        viewModel.startWebAuthenticationLogin(anchor: view.window)
    }
}
