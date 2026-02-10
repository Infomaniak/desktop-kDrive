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

public struct InformationBlockContentView: View {
    let icon: Image?
    let title: AttributedString
    let subtitle: AttributedString
    let button: InformationBlockButton?
    let dismissHandler: (() -> Void)?

    public init(
        icon: Image? = nil,
        title: AttributedString,
        subtitle: AttributedString,
        button: InformationBlockButton? = nil,
        dismissHandler: (() -> Void)? = nil
    ) {
        self.icon = icon
        self.title = title
        self.subtitle = subtitle
        self.button = button
        self.dismissHandler = dismissHandler
    }

    public init(
        icon: Image? = nil,
        title: String,
        subtitle: String,
        button: InformationBlockButton? = nil,
        dismissHandler: (() -> Void)? = nil
    ) {
        self.init(
            icon: icon,
            title: AttributedString(title),
            subtitle: AttributedString(subtitle),
            button: button,
            dismissHandler: dismissHandler
        )
    }

    public var body: some View {
        HStack(spacing: AppPadding.padding16) {
            HStack(alignment: .top, spacing: AppPadding.padding8) {
                if let icon {
                    RoundedRectangle(cornerRadius: AppRadius.radius4)
                        .fill(ColorToken.Accent.secondary.asColor)
                        .frame(width: 24, height: 24)
                        .overlay {
                            icon
                                .resizable(at: AppIconSize.iconSize12)
                                .foregroundStyle(.white)
                        }
                }

                VStack(alignment: .leading, spacing: AppPadding.padding2) {
                    Text(title)
                        .foregroundStyle(ColorToken.Text.primary.asColor)

                    Text(subtitle)
                        .foregroundStyle(ColorToken.Text.tertiary.asColor)
                }
                .multilineTextAlignment(.leading)
                .frame(maxWidth: .infinity, alignment: .leading)
            }

            HStack(spacing: AppPadding.padding8) {
                if let button {
                    Button(button.title, action: button.action)
                        .buttonStyle(.borderedProminent)
                }

                if let dismissHandler {
                    Button(action: dismissHandler) {
                        Label { Text(KDriveLocalizable.buttonClose) } icon: { Image(systemName: "xmark") }
                            .labelStyle(.iconOnly)
                            .foregroundStyle(ColorToken.Text.tertiary.asColor)
                            .padding(AppPadding.padding4)
                            .frame(maxHeight: .infinity)
                            .aspectRatio(1, contentMode: .fit)
                            .background(ColorToken.Surface.tertiary.asColor, in: .circle)
                    }
                    .buttonStyle(.plain)
                }
            }
            .fixedSize(horizontal: true, vertical: true)
        }
    }
}

#Preview("Title & Subtitle") {
    InformationBlockContentView(title: "This is a title", subtitle: "This is a subtitle")
}

#Preview("Icon & Title & Subtitle") {
    InformationBlockContentView(
        icon: KDriveResources.checkmark.swiftUIImage,
        title: "This is a title",
        subtitle: "This is a subtitle"
    )
}

#Preview("Icon & Title & Subtitle & Button") {
    InformationBlockContentView(
        icon: KDriveResources.circularArrowsClockwise.swiftUIImage,
        title: "This is a title",
        subtitle: "This is a subtitle",
        button: InformationBlockButton(title: "Click Me") {}
    )
}

#Preview("Icon & Title & Subtitle & Button | Dismiss") {
    InformationBlockContentView(
        icon: KDriveResources.circularArrowsClockwise.swiftUIImage,
        title: "This is a title",
        subtitle: "This is a subtitle",
        button: InformationBlockButton(title: "Click Me") {}
    ) { /* Dismiss handler */ }
}
