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
            fatalError()
        case .fail:
            fatalError()
        }
    }

    private func setupInitialView() {
        titleLabel.stringValue = "!Bienvenue dans kDrive !"
        descriptionLabel.stringValue = "!Le cloud privé, rapide et sécurisé, hébergé en Suisse.\n\nConnectez-vous et gardez vos documents synchronisés sur tous vos appareils."

        primaryButton.title = "!Se connecter"
        secondaryButton.title = "!Créer un compte"
    }
}
