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

import SwiftUI

public struct IKLabeledContent<Content: View>: View {
    let titleKey: LocalizedStringKey
    let content: Content

    public init(_ titleKey: LocalizedStringKey, @ViewBuilder content: () -> Content) {
        self.titleKey = titleKey
        self.content = content()
    }

    public var body: some View {
        if #available(macOS 13.0, *) {
            LabeledContent(titleKey) {
                content
            }
        } else {
            HStack {
                Text(titleKey)
                    .frame(maxWidth: .infinity, alignment: .leading)

                content
            }
        }
    }
}

#Preview {
    IKLabeledContent("Hello") {
        Text("World")
    }
}
