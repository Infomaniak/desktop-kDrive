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

public struct InformationBlockButton: Sendable {
    let title: String
    let action: @MainActor () -> Void

    public init(title: String, action: @escaping @MainActor () -> Void) {
        self.title = title
        self.action = action
    }
}

public struct InformationBlockView: View {
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
        InformationBlockContentView(icon: icon, title: title, subtitle: subtitle, button: button, dismissHandler: dismissHandler)
            .padding(AppPadding.padding16)
            .background(ColorToken.Surface.primary.asColor, in: .rect(cornerRadius: AppRadius.radius12))
    }
}

#Preview("Title & Subtitle") {
    InformationBlockView(title: "This is a title", subtitle: "This is a subtitle")
}

#Preview("Icon & Title & Subtitle") {
    InformationBlockView(icon: KDriveResources.checkmark.swiftUIImage, title: "This is a title", subtitle: "This is a subtitle")
}

#Preview("Icon & Title & Subtitle & Button") {
    InformationBlockView(
        icon: KDriveResources.circularArrowsClockwise.swiftUIImage,
        title: "This is a title",
        subtitle: "This is a subtitle",
        button: InformationBlockButton(title: "Click Me") {}
    )
}

#Preview("Icon & Title & Subtitle & Button | Dismiss") {
    InformationBlockView(
        icon: KDriveResources.circularArrowsClockwise.swiftUIImage,
        title: "This is a title",
        subtitle: "This is a subtitle",
        button: InformationBlockButton(title: "Click Me") {}
    ) { /* Dismiss handler */ }
}
