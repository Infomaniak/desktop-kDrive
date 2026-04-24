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

public struct IKContentUnavailableView: View {
    public struct Action: Sendable {
        let title: String
        let action: @Sendable () -> Void

        public init(title: String, action: @Sendable @escaping () -> Void) {
            self.title = title
            self.action = action
        }
    }

    let image: Image
    let title: String
    let subtitle: String
    let action: IKContentUnavailableView.Action?

    public init(image: Image, title: String, subtitle: String, action: IKContentUnavailableView.Action? = nil) {
        self.image = image
        self.title = title
        self.subtitle = subtitle
        self.action = action
    }

    public var body: some View {
        VStack(spacing: AppPadding.padding32) {
            image
                .resizable()
                .scaledToFit()
                .frame(maxWidth: 200)

            VStack(spacing: AppPadding.padding8) {
                Text(title)
                    .font(.Tokens.title3)
                    .foregroundStyle(ColorToken.Text.primary.asColor)

                Text(subtitle)
                    .font(.Tokens.body)
                    .foregroundStyle(ColorToken.Text.primary.asColor)
                    .fixedSize()

                if let action {
                    Button(action.title, action: action.action)
                        .buttonStyle(.borderless)
                        .tint(.accentColor)
                }
            }
        }
        .multilineTextAlignment(.center)
        .padding(AppPadding.padding24)
    }
}

#Preview("No Button") {
    IKContentUnavailableView(
        image: KDriveResources.mountainsTreesSun.swiftUIImage,
        title: "Nothing to see here",
        subtitle: "This is just a description"
    )
}

#Preview("With Button") {
    IKContentUnavailableView(
        image: KDriveResources.mountainsTreesSun.swiftUIImage,
        title: "Nothing to see here",
        subtitle: "This is just a description",
        action: .init(title: "Click Here") {}
    )
}
