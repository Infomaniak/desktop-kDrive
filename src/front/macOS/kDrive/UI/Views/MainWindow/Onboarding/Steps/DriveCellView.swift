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
import kDriveCore
import kDriveCoreUI

final class DriveCellView: NSView {
    let color: NSColor
    let title: String
    let subtitle: String?

    private lazy var checkbox: NSButton = {
        let checkboxButton = NSButton(checkboxWithTitle: "", target: self, action: #selector(didTapCheckbox))
        checkboxButton.translatesAutoresizingMaskIntoConstraints = false
        return checkboxButton
    }()

    private lazy var driveIcon: DriveSquareView = {
        let driveSquareView = DriveSquareView(color: color)
        driveSquareView.translatesAutoresizingMaskIntoConstraints = false
        return driveSquareView
    }()

    private lazy var titleLabel: NSTextField = {
        let textField = NSTextField(labelWithString: title)
        textField.translatesAutoresizingMaskIntoConstraints = false
        textField.font = NSFont.Tokens.body
        textField.textColor = NSColor.Tokens.Text.primary
        return textField
    }()

    private lazy var subtitleLabel: NSTextField? = {
        guard let subtitle else { return nil }

        let textField = NSTextField(labelWithString: subtitle)
        textField.translatesAutoresizingMaskIntoConstraints = false
        textField.font = NSFont.Tokens.subheadline
        textField.textColor = NSColor.Tokens.Text.tertiary
        return textField
    }()

    init(color: NSColor, title: String, subtitle: String? = nil) {
        self.color = color
        self.title = title
        self.subtitle = subtitle

        super.init(frame: .zero)
        setupView()
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func draw(_ dirtyRect: NSRect) {
        let path = NSBezierPath(roundedRect: bounds, xRadius: AppRadius.radius8, yRadius: AppRadius.radius8)
        NSColor.Tokens.Surface.secondary.setFill()
        path.fill()
    }

    private func setupView() {
        addSubview(checkbox)
        addSubview(driveIcon)
        addSubview(titleLabel)

        NSLayoutConstraint.activate([
            checkbox.centerYAnchor.constraint(equalTo: titleLabel.centerYAnchor),
            checkbox.leadingAnchor.constraint(equalTo: leadingAnchor, constant: AppPadding.padding8),

            driveIcon.centerYAnchor.constraint(equalTo: titleLabel.centerYAnchor),
            driveIcon.leadingAnchor.constraint(equalTo: checkbox.trailingAnchor, constant: AppPadding.padding8),

            titleLabel.leadingAnchor.constraint(equalTo: driveIcon.trailingAnchor, constant: AppPadding.padding8),
            titleLabel.topAnchor.constraint(equalTo: topAnchor, constant: AppPadding.padding8)
        ])

        if let subtitleLabel {
            addSubview(subtitleLabel)
            NSLayoutConstraint.activate([
                subtitleLabel.leadingAnchor.constraint(equalTo: titleLabel.leadingAnchor),
                subtitleLabel.topAnchor.constraint(equalTo: titleLabel.bottomAnchor, constant: AppPadding.padding2),
                bottomAnchor.constraint(equalTo: subtitleLabel.bottomAnchor, constant: AppPadding.padding8)
            ])
        } else {
            NSLayoutConstraint.activate([
                bottomAnchor.constraint(equalTo: titleLabel.bottomAnchor, constant: AppPadding.padding8)
            ])
        }
    }

    @objc private func didTapCheckbox() {}
}

@available(macOS 14.0, *)
#Preview {
    let driveCell = DriveCellView(color: .systemBlue, title: "Mon entreprise", subtitle: "kDrive Pro")
    driveCell.translatesAutoresizingMaskIntoConstraints = false
    return driveCell
}
