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

struct TemplateListRow: View {
    let item: UIExclusionTemplateInfo

    var body: some View {
        HStack {
            TextWithIcon(icon: KDriveResources.file.swiftUIImage, text: item.template)
                .frame(maxWidth: .infinity, alignment: .leading)
        }
    }
}

struct SynchroRulesPreferencesDefaultTemplateList: View {
    @Binding var defaultExcludedTemplates: [UIExclusionTemplateInfo]
    var body: some View {
        VStack(alignment: .leading) {
            ScrollView {
                ForEach(defaultExcludedTemplates) { item in
                    TemplateListRow(item: item)
                }
            }
            .padding(AppPadding.padding12)
            .background(.quinary)
            .cornerRadius(8)
        }
        .frame(maxWidth: .infinity)
    }
}

#Preview {
    SynchroRulesPreferencesDefaultTemplateList(defaultExcludedTemplates: .constant([]))
}
