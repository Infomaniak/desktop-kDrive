//
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

import kDriveCoreUI
import SwiftUI

struct UserHeaderCellView: View {
    let avatar: Image?
    let name: String
    let email: String

    var body: some View {
        HStack(spacing: AppPadding.padding8) {
            if let avatar {
                AvatarView(image: avatar)
                    .frame(width: 26, height: 26)
            }

            VStack(alignment: .leading) {
                Text(name)
                    .font(.Tokens.body)
                    .foregroundStyle(ColorToken.Text.primary.asColor)
                Text(email)
                    .font(.Tokens.subheadline)
                    .foregroundStyle(ColorToken.Text.tertiary.asColor)
            }
        }
    }
}

#Preview {
    UserHeaderCellView(avatar: nil, name: "Tim Cook", email: "tim@apple.com")
}
