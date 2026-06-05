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

import SwiftUI

public struct TagView: View {
    let text: String

    public init(text: String) {
        self.text = text
    }

    public var body: some View {
        Text(text)
            .font(.Tokens.callout)
            .padding(.horizontal, AppPadding.padding4)
            .padding(.vertical, AppPadding.padding2)
            .foregroundStyle(ColorToken.Status.Strong.success.asColor)
            .background(ColorToken.Status.Light.success.asColor, in: .rect(cornerRadius: 4))
    }
}

#Preview {
    TagView(text: "Example Tag")
        .padding()
}
