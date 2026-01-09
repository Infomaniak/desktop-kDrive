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

import kDriveResources
import SwiftUI

public struct UserAndDriveView: View {
    let avatar: Image?
    let driveColor: Color?

    public init(avatar: Image?, driveColor: Color?) {
        self.avatar = avatar
        self.driveColor = driveColor
    }

    public var body: some View {
        ZStack(alignment: .bottomTrailing) {
            avatarView
                .frame(width: 40, height: 40)
                .clipShape(Circle())
                .overlay {
                    Circle()
                        .stroke(ColorToken.Action.primary.asColor, lineWidth: 1)
                }
                .padding(AppPadding.padding4)

            Circle()
                .fill(Color(nsColor: .windowBackgroundColor))
                .frame(width: 20, height: 20)
                .overlay {
                    KDriveResources.kdriveFoldersStacked.swiftUIImage
                        .resizable()
                        .scaledToFit()
                        .frame(width: 12, height: 12)
                        .foregroundStyle(driveColor ?? ColorToken.Drive.defaultColor.asColor)
                }
        }
    }

    @ViewBuilder
    private var avatarView: some View {
        if let avatar {
            avatar
                .resizable()
                .scaledToFill()
        } else {
            ColorToken.Surface.tertiary.asColor
        }
    }
}

#Preview("Default") {
    UserAndDriveView(avatar: Image(nsImage: PreviewHelper.userImage), driveColor: .yellow)
}

#Preview("No Data") {
    UserAndDriveView(avatar: nil, driveColor: nil)
}
