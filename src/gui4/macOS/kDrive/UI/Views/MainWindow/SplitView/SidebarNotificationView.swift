/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import Cocoa
import kDriveCoreUI

final class SidebarNotificationView: NSView {
    static let defaultColor = ColorToken.Text.tertiary.asNSColor

    private lazy var imageView: NSImageView = {
        let imageView = NSImageView()
        imageView.translatesAutoresizingMaskIntoConstraints = false
        imageView.imageScaling = .scaleProportionallyDown
        NSLayoutConstraint.activate([
            imageView.widthAnchor.constraint(equalToConstant: AppIconSize.iconSize12.width),
            imageView.heightAnchor.constraint(equalToConstant: AppIconSize.iconSize12.height)
        ])
        return imageView
    }()

    private lazy var textField: NSTextField = {
        let textField = NSTextField(labelWithString: "")
        textField.translatesAutoresizingMaskIntoConstraints = false
        textField.font = NSFont.Tokens.body
        textField.textColor = ColorToken.Text.tertiary.asNSColor
        textField.alignment = .center
        return textField
    }()

    private lazy var labelRow: NSStackView = {
        let stackView = NSStackView(views: [imageView, textField])
        stackView.translatesAutoresizingMaskIntoConstraints = false
        stackView.orientation = .horizontal
        stackView.alignment = .centerY
        stackView.spacing = AppPadding.padding4
        return stackView
    }()

    private lazy var progressIndicator: NSProgressIndicator = {
        let indicator = NSProgressIndicator()
        indicator.translatesAutoresizingMaskIntoConstraints = false
        indicator.isIndeterminate = true
        indicator.style = .bar
        indicator.controlSize = .small
        indicator.startAnimation(nil)
        return indicator
    }()

    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        setupView()
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    private func setupView() {
        let stackView = NSStackView(views: [labelRow, progressIndicator])
        stackView.translatesAutoresizingMaskIntoConstraints = false
        stackView.orientation = .vertical
        stackView.alignment = .centerX
        stackView.spacing = AppPadding.padding8

        addSubview(stackView)

        NSLayoutConstraint.activate([
            stackView.topAnchor.constraint(equalTo: topAnchor),
            stackView.bottomAnchor.constraint(equalTo: bottomAnchor),
            stackView.leadingAnchor.constraint(equalTo: leadingAnchor),
            stackView.trailingAnchor.constraint(equalTo: trailingAnchor)
        ])
    }

    func configure(with state: SidebarNotificationState?) {
        let hasLabel = state?.icon != nil || state?.text != nil
        labelRow.isHidden = !hasLabel

        imageView.isHidden = state?.icon == nil
        if let icon = state?.icon {
            imageView.image = icon.icon
            imageView.contentTintColor = icon.tint ?? SidebarNotificationView.defaultColor
        }

        textField.isHidden = state?.text == nil
        if let text = state?.text {
            textField.stringValue = text.text
            textField.textColor = text.color ?? SidebarNotificationView.defaultColor
        }

        progressIndicator.isHidden = state?.showLoader != true
    }
}

@available(macOS 14.0, *)
#Preview {
    let notificationView = SidebarNotificationView()
    notificationView.translatesAutoresizingMaskIntoConstraints = false
    notificationView.configure(with: SidebarNotificationState(text: .init(text: "Hello, World!"), showLoader: true))

    NSLayoutConstraint.activate([
        notificationView.widthAnchor.constraint(equalToConstant: 200)
    ])

    return notificationView
}
