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
import kDriveResources
import SwiftUI

struct AppListRow: View {
    let item: UIExclusionAppInfo

    var body: some View {
        HStack {
            TextWithIcon(icon: KDriveResources.file.swiftUIImage, text: item.description)
                .frame(maxWidth: .infinity, alignment: .leading)

            Text(item.app)
                .frame(maxWidth: .infinity, alignment: .leading)
                .foregroundStyle(.secondary)
        }
    }
}

struct SynchroRulesPreferencesDefaultAppList: View {
    @Binding var defaultExcludedApps: [UIExclusionAppInfo]
    var body: some View {
        VStack(alignment: .leading) {
            HStack {
                Text(KDriveLocalizable.labelName)
                    .frame(maxWidth: .infinity, alignment: .leading)

                Divider()

                Text("Identifiant")
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .foregroundStyle(.secondary)
            }
            .frame(maxHeight: 24)
            .padding(.trailing, AppPadding.padding16)

            Divider()

            ScrollView {
                ForEach(defaultExcludedApps) { item in
                    AppListRow(item: item)
                }
            }
        }
        .padding(AppPadding.padding12)
        .background(.quinary)
        .cornerRadius(8)
    }
}

#Preview {
    SynchroRulesPreferencesDefaultAppList(defaultExcludedApps: .constant([]))
}
