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

class LabeledAvatarView: NSView {
    let name: String
    let avatar: NSImage?

    init(user: UIUser) {
        name = user.name
        avatar = user.avatar
        super.init(frame: .zero)
        setupView()
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    private func setupView() {
        let stackView = NSStackView()
        stackView.translatesAutoresizingMaskIntoConstraints = false
        stackView.alignment = .centerY
        stackView.spacing = AppPadding.padding8
        addSubview(stackView)

        if let avatar {
            let imageView = ResizableImageView(image: avatar)
            imageView.translatesAutoresizingMaskIntoConstraints = false
            imageView.preferredRect = NSRect(x: 0, y: 0, width: 24, height: 24)
            stackView.addArrangedSubview(imageView)

            NSLayoutConstraint.activate([
                imageView.widthAnchor.constraint(equalToConstant: imageView.preferredRect.width),
                imageView.heightAnchor.constraint(equalToConstant: imageView.preferredRect.height)
            ])
        }

        let label = NSTextField(labelWithString: name)
        label.translatesAutoresizingMaskIntoConstraints = false
        label.textColor = NSColor.Tokens.Text.secondary
        label.font = NSFont.Tokens.body
        stackView.addArrangedSubview(label)

        NSLayoutConstraint.activate([
            stackView.topAnchor.constraint(equalTo: topAnchor),
            stackView.bottomAnchor.constraint(equalTo: bottomAnchor),
            stackView.leadingAnchor.constraint(equalTo: leadingAnchor),
            stackView.trailingAnchor.constraint(equalTo: trailingAnchor)
        ])
    }
}

@available(macOS 14.0, *)
#Preview {
    LabeledAvatarView(user: PreviewHelper.user)
}
