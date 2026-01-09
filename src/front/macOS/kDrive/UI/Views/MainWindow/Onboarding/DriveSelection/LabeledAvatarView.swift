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
    var user: UIUser? {
        didSet {
            update(for: user)
        }
    }

    private lazy var imageView: ResizableImageView = {
        let resizableImage = ResizableImageView()
        resizableImage.translatesAutoresizingMaskIntoConstraints = false
        resizableImage.preferredRect = NSRect(x: 0, y: 0, width: 24, height: 24)
        resizableImage.layer?.cornerRadius = resizableImage.preferredRect.width / 2
        return resizableImage
    }()

    private lazy var label: NSTextField = {
        let textField = NSTextField(labelWithString: user?.name ?? "")
        textField.translatesAutoresizingMaskIntoConstraints = false
        textField.textColor = ColorToken.Text.secondary.asNSColor
        textField.font = NSFont.Tokens.body
        return textField
    }()

    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        setupView()
    }

    convenience init(user: UIUser? = nil) {
        self.init(frame: .zero)

        self.user = user
        setupView()
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
        setupView()
    }

    private func setupView() {
        let stackView = NSStackView(views: [imageView, label])
        stackView.translatesAutoresizingMaskIntoConstraints = false
        stackView.alignment = .centerY
        stackView.spacing = AppPadding.padding8
        addSubview(stackView)

        if let avatar = user?.nsAvatar {
            imageView.image = avatar
        }

        NSLayoutConstraint.activate([
            imageView.widthAnchor.constraint(equalToConstant: imageView.preferredRect.width),
            imageView.heightAnchor.constraint(equalToConstant: imageView.preferredRect.height),

            stackView.topAnchor.constraint(equalTo: topAnchor),
            stackView.bottomAnchor.constraint(equalTo: bottomAnchor),
            stackView.leadingAnchor.constraint(equalTo: leadingAnchor),
            stackView.trailingAnchor.constraint(equalTo: trailingAnchor)
        ])
    }

    private func update(for user: UIUser?) {
        guard let user else { return }

        label.stringValue = user.name
        if let avatar = user.nsAvatar {
            imageView.image = avatar
        }
    }
}

@available(macOS 14.0, *)
#Preview {
    LabeledAvatarView(user: PreviewHelper.user)
}
