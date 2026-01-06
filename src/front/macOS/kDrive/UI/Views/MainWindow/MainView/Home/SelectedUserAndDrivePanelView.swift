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

final class SelectedUserAndDrivePanelView: NSView {
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

        let favoriteFolderButton = generateLargeButton(
            icon: KDriveResources.star.image,
            title: "Favoris",
            action: #selector(openRemoteFavoriteFolder)
        )
        let sharedWithMeFolderButton = generateLargeButton(
            icon: KDriveResources.folderShare.image,
            title: "Partages",
            action: #selector(openRemoteSharedWithMeFolder)
        )
        let kDriveHomeFolderButton = generateLargeButton(
            icon: KDriveResources.kdriveFoldersStacked.image,
            title: "kDrive en ligne",
            action: #selector(openRemoteHomeFolder)
        )

        let buttonsStackView = NSStackView(views: [
            favoriteFolderButton,
            sharedWithMeFolderButton,
            kDriveHomeFolderButton
        ])
        buttonsStackView.translatesAutoresizingMaskIntoConstraints = false
        buttonsStackView.orientation = .vertical
        buttonsStackView.spacing = 8
        addSubview(buttonsStackView)
        
        let trashFolderButton = generateLargeButton(
            icon: KDriveResources.trash.image,
            title: "Corbeille",
            action: #selector(openRemoteTrashFolder)
        )
        addSubview(trashFolderButton)

        addSubview(avatarView)
        NSLayoutConstraint.activate([
            avatarView.centerXAnchor.constraint(equalTo: centerXAnchor),
            avatarView.topAnchor.constraint(equalTo: topAnchor, constant: AppPadding.padding24),
            avatarView.leadingAnchor.constraint(greaterThanOrEqualTo: leadingAnchor, constant: AppPadding.padding16),
            avatarView.trailingAnchor.constraint(lessThanOrEqualTo: trailingAnchor, constant: -AppPadding.padding16),

            buttonsStackView.topAnchor.constraint(equalTo: avatarView.bottomAnchor, constant: AppPadding.padding16),
            buttonsStackView.leadingAnchor.constraint(equalTo: leadingAnchor, constant: AppPadding.padding16),
            buttonsStackView.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -AppPadding.padding16),
            
            trashFolderButton.topAnchor.constraint(greaterThanOrEqualTo: buttonsStackView.bottomAnchor, constant: AppPadding.padding8),
            trashFolderButton.leadingAnchor.constraint(equalTo: leadingAnchor, constant: AppPadding.padding16),
            trashFolderButton.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -AppPadding.padding16),
            trashFolderButton.bottomAnchor.constraint(equalTo: bottomAnchor, constant: -AppPadding.padding16)
        ])
    }

    private func generateLargeButton(icon: NSImage, title: String, action: Selector) -> LargeButton {
        let button = LargeButton(icon: icon, title: title, accessory: KDriveResources.share.image)
        button.translatesAutoresizingMaskIntoConstraints = false
        button.target = self
        button.action = action
        return button
    }

    private func updateUser(for user: UIUser) {
        avatarView.user = user
    }

    private func updateDrive(for drive: UIDrive) {
        avatarView.drive = drive
    }
}

// MARK: - Actions

extension SelectedUserAndDrivePanelView {
    @objc private func openRemoteFavoriteFolder() {}

    @objc private func openRemoteSharedWithMeFolder() {}

    @objc private func openRemoteHomeFolder() {}

    @objc private func openRemoteTrashFolder() {}
}

@available(macOS 14.0, *)
#Preview {
    SelectedUserAndDrivePanelView(user: PreviewHelper.user, drive: PreviewHelper.drive1)
}
