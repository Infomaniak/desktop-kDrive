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
    private static let minContainerWidth: CGFloat = 300
    private static let maxContainerWidth: CGFloat = 500

    public let stackView = NSStackView()
    public let titleLabel = NSTextField(labelWithString: "")
    public let descriptionLabel = NSTextField(labelWithString: "")
    public let primaryButton = BorderedProminentButton()
    public let secondaryButton = BorderlessButton()

    private var customContent: NSView?

    override open func viewDidLoad() {
        super.viewDidLoad()
        setupComponents()
    }

    private func setupComponents() {
        stackView.translatesAutoresizingMaskIntoConstraints = false
        stackView.orientation = .vertical
        stackView.alignment = .leading
        view.addSubview(stackView)

        titleLabel.font = .preferredFont(forTextStyle: .largeTitle)
        titleLabel.textColor = .Tokens.Text.primary
        stackView.addArrangedSubview(titleLabel)
        stackView.setCustomSpacing(AppPadding.padding8, after: titleLabel)

        descriptionLabel.font = .preferredFont(forTextStyle: .body)
        descriptionLabel.lineBreakMode = .byWordWrapping
        descriptionLabel.textColor = .Tokens.Text.secondary
        descriptionLabel.setContentCompressionResistancePriority(.defaultLow, for: .horizontal)
        stackView.addArrangedSubview(descriptionLabel)
        stackView.setCustomSpacing(AppPadding.padding32, after: descriptionLabel)

        let buttonStack = NSStackView(views: [secondaryButton, primaryButton])
        buttonStack.spacing = AppPadding.padding8
        buttonStack.alignment = .centerY
        stackView.addArrangedSubview(buttonStack)

        secondaryButton.translatesAutoresizingMaskIntoConstraints = false

        primaryButton.translatesAutoresizingMaskIntoConstraints = false
        primaryButton.keyEquivalent = "\r"

        NSLayoutConstraint.activate([
            stackView.centerYAnchor.constraint(equalTo: view.centerYAnchor),
            stackView.widthAnchor.constraint(greaterThanOrEqualToConstant: Self.minContainerWidth),
            stackView.widthAnchor.constraint(lessThanOrEqualToConstant: Self.maxContainerWidth),

            stackView.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: AppPadding.padding64),
            view.trailingAnchor.constraint(greaterThanOrEqualTo: stackView.trailingAnchor, constant: AppPadding.padding64),
            stackView.topAnchor.constraint(greaterThanOrEqualTo: view.topAnchor, constant: AppPadding.padding32),
            view.bottomAnchor.constraint(greaterThanOrEqualTo: stackView.bottomAnchor, constant: AppPadding.padding32)
        ])
    }
}
