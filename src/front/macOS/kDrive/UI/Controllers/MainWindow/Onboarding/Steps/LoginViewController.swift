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

import AuthenticationServices
import Cocoa
import Combine
import InfomaniakDI
import InfomaniakLogin
import kDriveCoreUI
import kDriveResources

final class LoginViewController: OnboardingStepViewController, ASWebAuthenticationPresentationContextProviding {
    func presentationAnchor(for session: ASWebAuthenticationSession) -> ASPresentationAnchor {
        return view.window!
    }

    @LazyInjectService private var loginService: InfomaniakLoginable

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
        transitionContent(toStep: viewModel.currentStep)
        viewModel.$currentStep.receive(on: DispatchQueue.main).sink { [weak self] step in
            self?.transitionContent(toStep: step)
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

    @objc private func openLoginWebView() {
        loginService.asWebAuthenticationLoginFrom(
            anchor: view.window ?? ASPresentationAnchor(),
            useEphemeralSession: true,
            hideCreateAccountButton: true,
            delegate: self
        )
    }
}

extension LoginViewController: InfomaniakLoginDelegate {
    func didCompleteLoginWith(code: String, verifier: String) {
        print("Success")
    }

    func didFailLoginWith(error: any Error) {
        print("Failure")
    }
}
