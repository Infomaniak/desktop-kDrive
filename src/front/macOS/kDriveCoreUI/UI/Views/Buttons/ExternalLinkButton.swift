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

public struct ExternalLinkButtonStyle: ButtonStyle {
    public func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.Tokens.body)
            .foregroundStyle(ColorToken.Text.secondary.asColor)
            .padding(AppPadding.padding8)
            .frame(minHeight: 32)
            .background(ColorToken.Surface.primary.asColor)
            .clipShape(RoundedRectangle(cornerRadius: AppRadius.radius8))
            .opacity(configuration.isPressed ? 0.75 : 1.0)
    }
}

public struct ExternalLinkButton: View {
    let icon: Image
    let title: String
    let action: () -> Void

    public init(icon: Image, title: String, action: @escaping () -> Void) {
        self.icon = icon
        self.title = title
        self.action = action
    }

    public var body: some View {
        Button(action: action) {
            HStack(spacing: AppPadding.padding8) {
                icon
                    .resizable()
                    .scaledToFit()
                    .frame(width: 12, height: 12)
                    .foregroundStyle(ColorToken.Action.primary.asColor)

                Text(title)
                    .frame(maxWidth: .infinity, alignment: .leading)

                KDriveResources.squareArrowDiagonalUp.swiftUIImage
                    .resizable()
                    .scaledToFit()
                    .frame(width: 8, height: 8)
                    .foregroundStyle(ColorToken.Text.tertiary.asColor)
            }
        }
        .buttonStyle(ExternalLinkButtonStyle())
    }
}

#Preview {
    ExternalLinkButton(icon: KDriveResources.kdriveFoldersStacked.swiftUIImage, title: "Favoris") {}
}
