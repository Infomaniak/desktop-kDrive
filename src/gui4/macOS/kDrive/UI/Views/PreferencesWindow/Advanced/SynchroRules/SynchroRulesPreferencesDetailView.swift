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

struct SynchroRulesPreferencesDetailView: View {
    let item: SynchroRulesItem
    @ObservedObject var repository: ExclusionRepository

    @State private var isShowingSheet = false

    var body: some View {
        VStack(alignment: .leading, spacing: 24) {
            VStack(alignment: .leading, spacing: 16) {
                VStack(alignment: .leading, spacing: 8) {
                    Text(KDriveLocalizable.defaultExclusionFileListHeader)
                    Text(item.description)
                        .font(.Tokens.callout)
                        .foregroundStyle(.secondary)
                }

                if item == .apps {
                    SynchroRulesPreferencesDefaultAppList(defaultExcludedApps: $repository.exclusionInfo.defaultExcludedApps)
                } else {
                    SynchroRulesPreferencesDefaultTemplateList(defaultExcludedTemplates: $repository.exclusionInfo
                        .defaultExcludedTemplates)
                }
            }
            VStack(alignment: .leading, spacing: 16) {
                VStack(alignment: .leading, spacing: 8) {
                    Text(KDriveLocalizable.userExclusionFileListHeader)
                    Text(item.headerDescription)
                        .font(.Tokens.callout)
                        .foregroundStyle(.secondary)
                    Button(KDriveLocalizable.buttonAddFileExclusionRule) {
                        isShowingSheet = true
                    }
                    .buttonStyle(.borderedProminent)
                }

                if item == .apps {
                    SynchroRulesPreferencesUserAppList(
                        repository: repository,
                        userExcludedApps: $repository.exclusionInfo.userExcludedApps
                    )
                } else {
                    SynchroRulesPreferencesUserTemplateList(
                        repository: repository,
                        userExcludedTemplates: $repository.exclusionInfo.userExcludedTemplates
                    )
                }
            }
            .sheet(isPresented: $isShowingSheet) {
                SynchroRulesPreferencesSheet(
                    item: item,
                    repository: repository,
                    userExcludedApps: $repository.exclusionInfo.userExcludedApps,
                    userExcludedTemplates: $repository.exclusionInfo.userExcludedTemplates
                )
            }
        }
        .padding(AppPadding.padding24)
    }
}

#Preview {
    SynchroRulesPreferencesDetailView(item: .apps, repository: ExclusionRepository())
}
