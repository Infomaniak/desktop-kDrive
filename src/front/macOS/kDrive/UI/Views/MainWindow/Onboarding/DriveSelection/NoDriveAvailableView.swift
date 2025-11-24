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
import kDriveResources

final class NoDriveAvailableView: NSView {
    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        setupView()
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
        setupView()
    }

    private func setupView() {
        let titleLabel = NSTextField(labelWithString: KDriveLocalizable.onboardingDriveSelectionNoDriveTitle)
        titleLabel.translatesAutoresizingMaskIntoConstraints = false
        titleLabel.font = NSFont.Tokens.headline
        titleLabel.textColor = NSColor.Tokens.Text.secondary

        let descriptionLabel = NSTextField(labelWithString: KDriveLocalizable.onboardingDriveSelectionNoDriveDescription)
        descriptionLabel.translatesAutoresizingMaskIntoConstraints = false
        descriptionLabel.font = NSFont.Tokens.body
        descriptionLabel.textColor = NSColor.Tokens.Text.secondary

        let stackView = NSStackView(views: [titleLabel, descriptionLabel])
        stackView.translatesAutoresizingMaskIntoConstraints = false
        stackView.orientation = .vertical
        stackView.alignment = .leading
        stackView.spacing = AppPadding.padding8
        addSubview(stackView)

        NSLayoutConstraint.activate([
            stackView.leadingAnchor.constraint(equalTo: leadingAnchor),
            stackView.trailingAnchor.constraint(equalTo: trailingAnchor),
            stackView.topAnchor.constraint(equalTo: topAnchor),
            stackView.bottomAnchor.constraint(equalTo: bottomAnchor)
        ])
    }
}

@available(macOS 14.0, *)
#Preview {
    NoDriveAvailableView()
}
