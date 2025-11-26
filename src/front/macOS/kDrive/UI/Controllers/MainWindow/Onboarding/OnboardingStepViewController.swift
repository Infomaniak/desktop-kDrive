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
    enum Tokens {
        static let minContainerWidth: CGFloat = 300
        static let maxContainerWidth: CGFloat = 500

        static let viewHorizontalPadding: CGFloat = AppPadding.padding64
        static let viewVerticalPadding: CGFloat = AppPadding.padding32

        static let titlePadding: CGFloat = AppPadding.padding8
        static let descriptionPadding: CGFloat = AppPadding.padding32
        static let contentPadding: CGFloat = AppPadding.padding24
    }

    public let stackView = NSStackView()

    public let titleLabel = NSTextField(labelWithString: "")
    public let descriptionLabel = NSTextField(labelWithString: "")

    public let buttonsStack = NSStackView()
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

        titleLabel.font = NSFont.Tokens.largeTitleEmphasized
        titleLabel.textColor = NSColor.Tokens.Text.primary
        stackView.addArrangedSubview(titleLabel)
        stackView.setCustomSpacing(AppPadding.padding8, after: titleLabel)

        descriptionLabel.font = NSFont.Tokens.body
        descriptionLabel.lineBreakMode = .byWordWrapping
        descriptionLabel.textColor = NSColor.Tokens.Text.secondary
        descriptionLabel.setContentCompressionResistancePriority(.defaultLow, for: .horizontal)
        stackView.addArrangedSubview(descriptionLabel)
        stackView.setCustomSpacing(AppPadding.padding32, after: descriptionLabel)

        buttonsStack.spacing = AppPadding.padding8
        buttonsStack.alignment = .centerY
        stackView.addArrangedSubview(buttonsStack)

        buttonsStack.addArrangedSubview(secondaryButton)
        secondaryButton.translatesAutoresizingMaskIntoConstraints = false

        buttonsStack.addArrangedSubview(primaryButton)
        primaryButton.translatesAutoresizingMaskIntoConstraints = false
        primaryButton.keyEquivalent = "\r"

        NSLayoutConstraint.activate([
            stackView.centerYAnchor.constraint(equalTo: view.centerYAnchor),
            stackView.widthAnchor.constraint(greaterThanOrEqualToConstant: Self.Tokens.minContainerWidth),
            stackView.widthAnchor.constraint(lessThanOrEqualToConstant: Self.Tokens.maxContainerWidth),

            stackView.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: Self.Tokens.viewHorizontalPadding),
            view.trailingAnchor.constraint(
                greaterThanOrEqualTo: stackView.trailingAnchor,
                constant: Self.Tokens.viewHorizontalPadding
            ),
            stackView.topAnchor.constraint(greaterThanOrEqualTo: view.topAnchor, constant: Self.Tokens.viewVerticalPadding),
            view.bottomAnchor.constraint(greaterThanOrEqualTo: stackView.bottomAnchor, constant: Self.Tokens.viewVerticalPadding)
        ])
    }
}
