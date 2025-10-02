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

final class LoginViewController: NSViewController {
    private let viewModel: OnboardingViewModel

    private let containerView = NSView()
    private var titleLabel = NSTextField(labelWithString: "")
    private var descriptionLabel = NSTextField(labelWithString: "")
    private var loginButton = NSButton()
    private var createAccountButton = NSButton()

    init(viewModel: OnboardingViewModel) {
        self.viewModel = viewModel
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func viewDidLoad() {
        super.viewDidLoad()

        setupComponents()
        setupInitialView()
    }

    private func setupComponents() {
        containerView.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(containerView)

        titleLabel.translatesAutoresizingMaskIntoConstraints = false
        titleLabel.font = .preferredFont(forTextStyle: .largeTitle)
        // TODO: Set foreground
        containerView.addSubview(titleLabel)

        descriptionLabel.translatesAutoresizingMaskIntoConstraints = false
        descriptionLabel.font = .preferredFont(forTextStyle: .body)
        // TODO: Set foreground
        containerView.addSubview(descriptionLabel)

        loginButton.translatesAutoresizingMaskIntoConstraints = false
        containerView.addSubview(loginButton)

        createAccountButton.translatesAutoresizingMaskIntoConstraints = false
        containerView.addSubview(createAccountButton)

        NSLayoutConstraint.activate([
            containerView.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            containerView.centerYAnchor.constraint(equalTo: view.centerYAnchor),
            
        ])
    }

    private func setupInitialView() {
        titleLabel.stringValue = "!Bienvenue dans kDrive !"
        descriptionLabel.stringValue = "!Le cloud privé, rapide et sécurisé, hébergé en Suisse.\n\nConnectez-vous et gardez vos documents synchronisés sur tous vos appareils."

        loginButton.title = "Se connecter"
        createAccountButton.title = "Créer un compte"
    }
}
