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
import kDriveResources
import SwiftUI

struct SynchroFolderTypeLabel: View {
    let text: String

    var body: some View {
        HStack(spacing: AppPadding.padding8) {
            KDriveResources.folderFilled.swiftUIImage
                .resizable(at: AppIconSize.iconSize12)
                .foregroundStyle(ColorToken.Surface.quaternary.asColor)
            Text(text)
                .font(.Tokens.bodyEmphasized)
                .foregroundStyle(ColorToken.Text.primary.asColor)
        }
        .padding(AppPadding.padding8)
        .background(Color.white, in: .rect(cornerRadius: AppRadius.radius8))
    }
}

#Preview {
    SynchroFolderTypeLabel(text: "Hello, World")
}
