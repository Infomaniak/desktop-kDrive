//
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

final class SelectedUserAndDriveView: NSView {
    var user: UIUser {
        didSet {
            updateUser(for: user)
        }
    }

    var drive: UIDrive {
        didSet {
            updateDrive(for: drive)
        }
    }

    private lazy var avatarView: UserAvatarAndDriveView = {
        let avatarView = UserAvatarAndDriveView(user: user, drive: drive)
        avatarView.translatesAutoresizingMaskIntoConstraints = false
        return avatarView
    }()

    init(user: UIUser, drive: UIDrive) {
        self.user = user
        self.drive = drive
        super.init(frame: .zero)

        setupView()
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    private func setupView() {
        wantsLayer = true
        layer?.backgroundColor = NSColor.Tokens.Surface.secondary.cgColor
        layer?.cornerRadius = AppRadius.radius16

        addSubview(avatarView)
        NSLayoutConstraint.activate([
            avatarView.centerXAnchor.constraint(equalTo: centerXAnchor),
            avatarView.topAnchor.constraint(equalTo: topAnchor, constant: AppPadding.padding24),
            avatarView.leadingAnchor.constraint(greaterThanOrEqualTo: leadingAnchor, constant: AppPadding.padding16),
            avatarView.trailingAnchor.constraint(lessThanOrEqualTo: trailingAnchor, constant: -AppPadding.padding16)
        ])
    }

    private func updateUser(for user: UIUser) {
        avatarView.user = user
    }

    private func updateDrive(for drive: UIDrive) {
        avatarView.drive = drive
    }
}

@available(macOS 14.0, *)
#Preview {
    SelectedUserAndDriveView(user: PreviewHelper.user, drive: PreviewHelper.drive1)
}
