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
import kDriveResources

final class UserAvatarAndDriveView: NSView {
    static let avatarInset: CGFloat = 5
    static let avatarSize: CGFloat = 44
    static let driveIconSize: CGFloat = 12
    static let driveBackgroundSize: CGFloat = 20

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

    private lazy var avatarImageView: ResizableImageView = {
        let image = ResizableImageView(image: user.avatar)
        image.translatesAutoresizingMaskIntoConstraints = false
        image.wantsLayer = true
        image.layer?.cornerRadius = UserAvatarAndDriveView.avatarSize / 2
        image.layer?.borderWidth = 1
        image.layer?.borderColor = NSColor.Tokens.Action.primary.cgColor
        return image
    }()

    private lazy var circleBackgroundView: NSView = {
        let backgroundView = NSView()
        backgroundView.translatesAutoresizingMaskIntoConstraints = false
        backgroundView.wantsLayer = true
        backgroundView.layer?.backgroundColor = NSColor.Tokens.Surface.background.cgColor
        backgroundView.layer?.cornerRadius = UserAvatarAndDriveView.driveBackgroundSize / 2
        return backgroundView
    }()

    private lazy var driveIcon: NSImageView = {
        let imageView = NSImageView(image: KDriveResources.kdriveFoldersStacked.image)
        imageView.translatesAutoresizingMaskIntoConstraints = false
        return imageView
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
        addSubview(avatarImageView)

        addSubview(circleBackgroundView)
        addSubview(driveIcon)

        updateUser(for: user)
        updateDrive(for: drive)

        NSLayoutConstraint.activate([
            avatarImageView.widthAnchor.constraint(equalToConstant: UserAvatarAndDriveView.avatarSize),
            avatarImageView.heightAnchor.constraint(equalToConstant: UserAvatarAndDriveView.avatarSize),
            avatarImageView.topAnchor.constraint(equalTo: topAnchor, constant: UserAvatarAndDriveView.avatarInset),
            avatarImageView.leadingAnchor.constraint(equalTo: leadingAnchor, constant: UserAvatarAndDriveView.avatarInset),
            avatarImageView.bottomAnchor.constraint(equalTo: bottomAnchor, constant: -UserAvatarAndDriveView.avatarInset),
            avatarImageView.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -UserAvatarAndDriveView.avatarInset),

            circleBackgroundView.widthAnchor.constraint(equalToConstant: UserAvatarAndDriveView.driveBackgroundSize),
            circleBackgroundView.heightAnchor.constraint(equalToConstant: UserAvatarAndDriveView.driveBackgroundSize),
            circleBackgroundView.centerXAnchor
                .constraint(equalTo: avatarImageView.trailingAnchor, constant: -UserAvatarAndDriveView.avatarInset),
            circleBackgroundView
                .centerYAnchor.constraint(equalTo: avatarImageView.bottomAnchor, constant: -UserAvatarAndDriveView.avatarInset),

            driveIcon.widthAnchor.constraint(equalToConstant: UserAvatarAndDriveView.driveIconSize),
            driveIcon.heightAnchor.constraint(equalToConstant: UserAvatarAndDriveView.driveIconSize),
            driveIcon.centerXAnchor.constraint(equalTo: circleBackgroundView.centerXAnchor),
            driveIcon.centerYAnchor.constraint(equalTo: circleBackgroundView.centerYAnchor)
        ])
    }

    private func updateUser(for user: UIUser) {
        avatarImageView.image = user.avatar
    }

    private func updateDrive(for drive: UIDrive) {
        guard let driveColor = drive.color else {
            return
        }

        driveIcon.contentTintColor = driveColor
    }

}

@available(macOS 14.0, *)
#Preview {
    UserAvatarAndDriveView(user: PreviewHelper.user, drive: PreviewHelper.drive1)
}
