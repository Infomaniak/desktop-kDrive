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

import kDriveResources
import SwiftUI

public struct AnonymousAvatarView: View {
    public init() {}

    public var body: some View {
        KDriveResources.person.swiftUIImage
            .resizable()
            .scaledToFit()
            .frame(size: AppIconSize.iconSize16)
            .foregroundStyle(ColorToken.Text.tertiary.asColor)
            .overlay {
                Circle()
                    .stroke(ColorToken.Surface.quaternary.asColor, lineWidth: 1)
                    .frame(width: 24, height: 24)
            }
            .accessibilityHidden(true)
    }
}

#Preview {
    AnonymousAvatarView()
        .padding()
}
