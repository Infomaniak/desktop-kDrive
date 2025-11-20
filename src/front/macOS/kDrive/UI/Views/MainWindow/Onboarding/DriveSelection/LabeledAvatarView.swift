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

    public init(user: UIUser) {
        name = user.name
        avatar = user.avatar
        super.init(frame: .zero)
        setupView()
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    private func setupView() {
        let stackView = NSStackView()
        stackView.translatesAutoresizingMaskIntoConstraints = false
        stackView.alignment = .centerY
        addSubview(stackView)

        if let avatar {
            print(avatar)
            let imageView = NSImageView(image: avatar)
            imageView.translatesAutoresizingMaskIntoConstraints = false
            stackView.addArrangedSubview(imageView)
        }

        let label = NSTextField(labelWithString: name)
        label.translatesAutoresizingMaskIntoConstraints = false
        label.textColor = NSColor.Tokens.Text.secondary
        label.font = NSFont.Tokens.body
        stackView.addArrangedSubview(label)
    }
}

@available(macOS 14.0, *)
#Preview {
    LabeledAvatarView(user: PreviewHelper.user)
}
