/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2026 Infomaniak Network SA

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

import kDriveCoreUI
import kDriveResources
import SwiftUI

/// Small tag recalling the drive a storage item refers to.
struct StorageDriveTag: View {
    let drive: UIDrive?
    var synchro: UISynchro?

    /// The sync name uses the default color; the drive's own color only applies when its name is shown.
    private var iconColor: Color {
        guard synchro == nil, let driveColor = drive?.color else {
            return ColorToken.Drive.defaultColor.asColor
        }
        return driveColor
    }

    var body: some View {
        Label {
            Text(synchro?.localPath.lastPathComponent ?? drive?.name ?? "")
                .lineLimit(1)
        } icon: {
            KDriveResources.kdriveFoldersStackedFilled.swiftUIImage
                .resizable()
                .scaledToFit()
                .frame(width: 14, height: 14)
                .foregroundStyle(iconColor)
        }
        .labelStyle(.titleAndIcon)
        .padding(.horizontal, AppPadding.padding4)
        .padding(.vertical, AppPadding.padding2)
        .background(
            RoundedRectangle(cornerRadius: AppRadius.radius4)
                .fill(Color(nsColor: .windowBackgroundColor))
        )
    }
}

#Preview {
    StorageDriveTag(drive: UIDrive(dbId: 1, driveId: 1, name: "My kDrive", isAdmin: true, hexColor: nil))
        .padding()
}
