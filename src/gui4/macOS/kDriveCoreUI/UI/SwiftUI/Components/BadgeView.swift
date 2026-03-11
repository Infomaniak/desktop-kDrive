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

public struct BadgeView: View {
    let image: Image
    let color: Color

    let iconSize: CGFloat
    let squareSize: CGFloat
    let radius: CGFloat

    private var padding: CGFloat {
        return (squareSize - iconSize) / 2
    }

    public init(image: Image, color: Color, iconSize: CGFloat = 12, squareSize: CGFloat = 20, radius: CGFloat = AppRadius.radius4) {
        self.image = image
        self.color = color

        self.iconSize = iconSize
        self.squareSize = squareSize
        self.radius = radius
    }

    public var body: some View {
        if #available(macOS 26.0, *) {
            icon
                .padding(padding)
                .glassEffect(.regular.tint(color), in: .rect(cornerRadius: radius))
        } else {
            RoundedRectangle(cornerRadius: radius)
                .fill(color)
                .frame(width: squareSize, height: squareSize)
                .overlay { icon }
        }
    }

    private var icon: some View {
        image
            .resizable()
            .scaledToFit()
            .foregroundStyle(.white)
            .frame(width: iconSize, height: iconSize)
    }
}

#Preview {
    BadgeView(image: KDriveResources.kdriveFoldersStacked.swiftUIImage, color: .yellow)
        .padding()
}
