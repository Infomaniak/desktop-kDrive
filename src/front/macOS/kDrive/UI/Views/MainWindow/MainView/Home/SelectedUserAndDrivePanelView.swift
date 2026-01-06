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

enum RemoteFolder: Int {
    case favorites
    case sharedWithMe
    case kDriveHome
    case trash
    
    var name: String {
        switch self {
        case .favorites:
            return "Favoris"
        case .sharedWithMe:
            return "Partages"
        case .kDriveHome:
            return "kDrive en ligne"
        case .trash:
            return "Corbeille"
        }
    }
    
    var icon: NSImage {
        switch self {
        case .favorites:
            return KDriveResources.star.image
        case .sharedWithMe:
            return KDriveResources.folderShare.image
        case .kDriveHome:
            return KDriveResources.kdriveFoldersStacked.image
        case .trash:
            return KDriveResources.trash.image
        }
    }
}

protocol SelectedUserAndDrivePanelDelegate: AnyObject {
    func didTapRemoteFolderButton(_ remoteFolder: RemoteFolder)
}

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
    
    weak var delegate: SelectedUserAndDrivePanelDelegate?

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
        
        let topFolders: [RemoteFolder] = [.favorites, .sharedWithMe, .kDriveHome]
        let folderButtons = topFolders.map { generateLargeButton(for: $0) }

        let buttonsStackView = NSStackView(views: folderButtons)
        buttonsStackView.translatesAutoresizingMaskIntoConstraints = false
        buttonsStackView.orientation = .vertical
        buttonsStackView.spacing = 8
        addSubview(buttonsStackView)
        
        let trashFolderButton = generateLargeButton(for: .trash)
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

    private func generateLargeButton(for remoteFolder: RemoteFolder) -> LargeButton {
        let button = LargeButton(icon: remoteFolder.icon, title: remoteFolder.name, accessory: KDriveResources.share.image)
        button.translatesAutoresizingMaskIntoConstraints = false
        button.target = self
        button.action = #selector(didTapLargeButton)
        button.tag = remoteFolder.rawValue
        return button
    }

    private func updateUser(for user: UIUser) {
        avatarView.user = user
    }

    private func updateDrive(for drive: UIDrive) {
        avatarView.drive = drive
    }
    
    @objc private func didTapLargeButton(_ sender: LargeButton) {
        guard let remoteFolder = RemoteFolder(rawValue: sender.tag) else {
            return
        }
        
        delegate?.didTapRemoteFolderButton(remoteFolder)
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
