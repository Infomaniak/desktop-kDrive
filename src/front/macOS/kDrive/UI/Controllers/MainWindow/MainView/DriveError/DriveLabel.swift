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

import kDriveCore
import kDriveCoreUI
import kDriveResources
import SwiftUI

@available(macOS 12.0, *)
struct DriveLabelBadge: View {
    let icon: Image
    let backgroundColor: Color
    let foregroundColor: Color

    var body: some View {
        icon
            .resizable()
            .scaledToFit()
            .frame(width: 20, height: 20)
            .padding(AppPadding.padding8)
            .background(
                Circle().fill(backgroundColor)
            )
            .overlay(
                Circle()
                    .stroke(foregroundColor, lineWidth: 1)
            )
            .foregroundColor(foregroundColor)
    }
}

@available(macOS 12.0, *)
struct DriveLabel: View {
    let drive: UIDrive

    let badgeIcon: Image
    let badgeBackgroundColor: Color
    let badgeColor: Color

    var body: some View {
        Label {
            Text(drive.name)
                .font(.title3)
        } icon: {
            KDriveResources.kdriveFoldersStacked.swiftUIImage
                .foregroundStyle(drive.color ?? .accentColor)
        }
        .padding(.horizontal, AppPadding.padding24)
        .padding(.vertical, AppPadding.padding8)
        .background(
            RoundedRectangle(cornerRadius: AppRadius.radius8)
                .fill(Color(nsColor: .windowBackgroundColor))
        )
        .overlay(alignment: .topTrailing) {
            DriveLabelBadge(
                icon: badgeIcon,
                backgroundColor: badgeBackgroundColor,
                foregroundColor: badgeColor
            )
            .offset(x: AppPadding.padding16, y: -AppPadding.padding16)
        }
    }
}

@available(macOS 12.0, *)
#Preview {
    DriveLabel(
        drive: PreviewHelper.blockingError.drive,
        badgeIcon: PreviewHelper.blockingError.badgeIcon,
        badgeBackgroundColor: PreviewHelper.blockingError.badgeBackgroundColor,
        badgeColor: PreviewHelper.blockingError.badgeColor
    )
    .padding(32)
    .background(Color.gray)
}
