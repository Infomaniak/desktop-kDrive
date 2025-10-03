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
import kDriveCoreUI

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

    @available(*, unavailable)
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
        descriptionLabel.lineBreakMode = .byWordWrapping
        // TODO: Set foreground
        containerView.addSubview(descriptionLabel)

        loginButton.translatesAutoresizingMaskIntoConstraints = false
        containerView.addSubview(loginButton)

        createAccountButton.translatesAutoresizingMaskIntoConstraints = false
        containerView.addSubview(createAccountButton)

        NSLayoutConstraint.activate([
            containerView.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            containerView.centerYAnchor.constraint(equalTo: view.centerYAnchor),
            containerView.widthAnchor.constraint(greaterThanOrEqualToConstant: 200),
            containerView.widthAnchor.constraint(lessThanOrEqualToConstant: 450),

            containerView.leadingAnchor.constraint(greaterThanOrEqualTo: view.leadingAnchor, constant: AppPadding.padding16),
            view.trailingAnchor.constraint(greaterThanOrEqualTo: containerView.trailingAnchor, constant: AppPadding.padding16),
            containerView.topAnchor.constraint(greaterThanOrEqualTo: view.topAnchor, constant: AppPadding.padding16),
            view.bottomAnchor.constraint(greaterThanOrEqualTo: containerView.bottomAnchor, constant: AppPadding.padding16),

            titleLabel.topAnchor.constraint(equalTo: containerView.topAnchor),
            titleLabel.leadingAnchor.constraint(equalTo: containerView.leadingAnchor),
            titleLabel.trailingAnchor.constraint(lessThanOrEqualTo: containerView.trailingAnchor),

            descriptionLabel.topAnchor.constraint(equalTo: titleLabel.bottomAnchor, constant: AppPadding.padding8),
            descriptionLabel.leadingAnchor.constraint(equalTo: containerView.leadingAnchor),
            descriptionLabel.trailingAnchor.constraint(lessThanOrEqualTo: containerView.trailingAnchor),

            loginButton.topAnchor.constraint(equalTo: descriptionLabel.bottomAnchor, constant: AppPadding.padding32),
            loginButton.leadingAnchor.constraint(equalTo: containerView.leadingAnchor),
            containerView.bottomAnchor.constraint(equalTo: loginButton.bottomAnchor),

            createAccountButton.topAnchor.constraint(equalTo: descriptionLabel.bottomAnchor, constant: AppPadding.padding32),
            createAccountButton.leadingAnchor.constraint(equalTo: loginButton.trailingAnchor, constant: AppPadding.padding8),
            containerView.bottomAnchor.constraint(equalTo: createAccountButton.bottomAnchor)
        ])
    }

    private func setupInitialView() {
        titleLabel.stringValue = "!Bienvenue dans kDrive !"
        descriptionLabel.stringValue = "!Le cloud privé, rapide et sécurisé, hébergé en Suisse.\n\nConnectez-vous et gardez vos documents synchronisés sur tous vos appareils."

        loginButton.title = "Se connecter"
        createAccountButton.title = "Créer un compte"
    }
}
