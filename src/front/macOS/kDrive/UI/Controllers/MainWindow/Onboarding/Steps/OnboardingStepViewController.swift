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

open class OnboardingStepViewController: NSViewController {
    private static let minContainerWidth: CGFloat = 200
    private static let mawContainerWidth: CGFloat = 450

    public let containerView = NSView()
    public let titleLabel = NSTextField(labelWithString: "")
    public let descriptionLabel = NSTextField(labelWithString: "")
    public let primaryButton = BorderedProminentButton()
    public let secondaryButton = BorderlessButton()

    override open func viewDidLoad() {
        super.viewDidLoad()
        setupComponents()
    }

    private func setupComponents() {
        containerView.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(containerView)

        titleLabel.translatesAutoresizingMaskIntoConstraints = false
        titleLabel.font = .preferredFont(forTextStyle: .largeTitle)
        titleLabel.textColor = .Tokens.Text.primary
        containerView.addSubview(titleLabel)

        descriptionLabel.translatesAutoresizingMaskIntoConstraints = false
        descriptionLabel.font = .preferredFont(forTextStyle: .body)
        descriptionLabel.lineBreakMode = .byWordWrapping
        descriptionLabel.textColor = .Tokens.Text.secondary
        containerView.addSubview(descriptionLabel)

        secondaryButton.translatesAutoresizingMaskIntoConstraints = false
        containerView.addSubview(secondaryButton)

        primaryButton.translatesAutoresizingMaskIntoConstraints = false
        primaryButton.keyEquivalent = "\r"
        containerView.addSubview(primaryButton)

        NSLayoutConstraint.activate([
            containerView.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            containerView.centerYAnchor.constraint(equalTo: view.centerYAnchor),
            containerView.widthAnchor.constraint(greaterThanOrEqualToConstant: Self.minContainerWidth),
            containerView.widthAnchor.constraint(lessThanOrEqualToConstant: Self.mawContainerWidth),

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

            secondaryButton.topAnchor.constraint(equalTo: descriptionLabel.bottomAnchor, constant: AppPadding.padding32),
            secondaryButton.leadingAnchor.constraint(equalTo: containerView.leadingAnchor),
            containerView.bottomAnchor.constraint(equalTo: secondaryButton.bottomAnchor),

            primaryButton.topAnchor.constraint(equalTo: descriptionLabel.bottomAnchor, constant: AppPadding.padding32),
            primaryButton.leadingAnchor.constraint(equalTo: secondaryButton.trailingAnchor, constant: AppPadding.padding8),
            containerView.bottomAnchor.constraint(equalTo: primaryButton.bottomAnchor)
        ])
    }
}
