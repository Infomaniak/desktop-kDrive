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

import kDriveCore
import kDriveCoreUI
import kDriveResources
import SwiftUI

struct WebFolder: Sendable, Identifiable {
    let id: Int
    let name: String
    let icon: Image

    static let favorites = WebFolder(
        id: 0,
        name: KDriveLocalizable.folderFavorites,
        icon: KDriveResources.star.swiftUIImage
    )
    static let sharedWithMe = WebFolder(
        id: 1,
        name: KDriveLocalizable.folderShares,
        icon: KDriveResources.folderShare.swiftUIImage
    )
    static let kDriveHome = WebFolder(
        id: 2,
        name: KDriveLocalizable.buttonKDriveOnline,
        icon: KDriveResources.kdriveFoldersStacked.swiftUIImage
    )
    static let trash = WebFolder(
        id: 3,
        name: KDriveLocalizable.folderTrash,
        icon: KDriveResources.trash.swiftUIImage
    )
}

struct DriveWebShortcutsView: View {
    let user: UIUser
    let drive: UIDrive

    private var topFolders: [WebFolder] {
        return [.favorites, .sharedWithMe, .kDriveHome]
    }

    var body: some View {
        VStack(spacing: AppPadding.padding16) {
            UserAndDriveView(avatar: nil, driveColor: nil)

            VStack(spacing: AppPadding.padding8) {
                ForEach(topFolders) { folder in
                    ExternalLinkButton(icon: folder.icon, title: folder.name) {
                        didTapFolder(folder)
                    }
                }
            }
            .frame(maxHeight: .infinity, alignment: .top)

            ExternalLinkButton(icon: WebFolder.trash.icon, title: WebFolder.trash.name) {
                didTapFolder(WebFolder.trash)
            }
        }
        .padding(AppPadding.padding16)
        .background(ColorToken.Surface.secondary.asColor)
        .clipShape(RoundedRectangle(cornerRadius: AppRadius.radius16))
    }

    private func didTapFolder(_ folder: WebFolder) {
    }
}

#Preview {
    DriveWebShortcutsView(user: PreviewHelper.user, drive: PreviewHelper.drive1)
}
