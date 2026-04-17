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

import kDriveCoreUI
import kDriveResources
import SwiftUI

struct DataManagementPreferencesHeaderView: View {
    var body: some View {
        VStack(alignment: .leading, spacing: AppPadding.padding8) {
            Text(KDriveLocalizable.dataManagementSubtitle)
                .font(.Tokens.bodyEmphasized)
            Text(KDriveLocalizable.dataManagementDescription)
                .font(.Tokens.callout)
                .foregroundStyle(ColorToken.Text.tertiary.asColor)

            Link(KDriveLocalizable.viewSourceCode, destination: DataManagementPreferencesView.githubURL)
                .buttonStyle(.bordered)
                .foregroundStyle(ColorToken.Accent.primary.asColor)
        }
    }
}

#Preview {
    DataManagementPreferencesHeaderView()
}
