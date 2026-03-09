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

public struct DriveBadgeView: View {
    let color: Color

    public init(color: Color) {
        self.color = color
    }

    public var body: some View {
        if #available(macOS 26.0, *) {
            icon
                .padding(AppPadding.padding4)
                .glassEffect(.regular.tint(color), in: .rect(cornerRadius: AppRadius.radius4))
        } else {
            RoundedRectangle(cornerRadius: AppRadius.radius4)
                .fill(color)
                .frame(width: 20, height: 20)
                .overlay { icon }
        }
    }

    private var icon: some View {
        KDriveResources.kdriveFoldersStacked.swiftUIImage
            .resizable()
            .scaledToFit()
            .foregroundStyle(.white)
            .frame(width: 12, height: 12)
    }
}

#Preview {
    DriveBadgeView(color: .yellow)
        .padding()
}
