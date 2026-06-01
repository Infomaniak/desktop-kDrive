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

import kDriveCore
import kDriveCoreUI
import kDriveResources
import SwiftUI

struct SynchroRulesPreferencesDetailView: View {
    @State private var isShowingSheet = false
    @State private var appList: [String: String] = [:]

    @ObservedObject var repository: ExclusionRepository

    let item: SynchroRulesItem

    private var isButtonDisabled: Bool {
        return item == .apps && appList.isEmpty
    }

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
                    SynchroRulesPreferencesDefaultAppList(
                        defaultExcludedApps: repository.exclusionInfo.defaultExcludedApps
                    )
                } else {
                    SynchroRulesPreferencesDefaultTemplateList(
                        defaultExcludedTemplates: repository.exclusionInfo.defaultExcludedTemplates
                    )
                }
            }

            VStack(alignment: .leading, spacing: AppPadding.padding16) {
                VStack(alignment: .leading, spacing: AppPadding.padding8) {
                    Text(KDriveLocalizable.userExclusionFileListHeader)

                    Text(item.headerDescription)
                        .font(.Tokens.callout)
                        .foregroundStyle(.secondary)

                    Button(KDriveLocalizable.buttonAddFileExclusionRule) {
                        isShowingSheet = true
                    }
                    .buttonStyle(.borderedProminent)
                    .disabled(isButtonDisabled)
                }

                if item == .apps {
                    SynchroRulesPreferencesUserAppList(
                        userExcludedApps: $repository.exclusionInfo.userExcludedApps,
                        repository: repository
                    )
                } else {
                    SynchroRulesPreferencesUserTemplateList(
                        userExcludedTemplates: $repository.exclusionInfo.userExcludedTemplates,
                        repository: repository
                    )
                }
            }
            .sheet(isPresented: $isShowingSheet) {
                SynchroRulesPreferencesSheet(
                    userExcludedApps: $repository.exclusionInfo.userExcludedApps,
                    userExcludedTemplates: $repository.exclusionInfo.userExcludedTemplates,
                    appList: $appList,
                    item: item,
                    repository: repository
                )
            }
        }
        .padding(AppPadding.page)
        .onAppear(perform: getAppList)
    }

    func getAppList() {
        Task {
            do {
                let userExcludedAppIdentifiers = repository.exclusionInfo.userExcludedApps.map(\.app)
                let dict = try await ExclusionAppJobs().getFetchingAppList()
                    .filter { !userExcludedAppIdentifiers.contains($0.key) }

                appList = dict
            } catch {
                print("Error while fetching app list: \(error)")
            }
        }
    }
}

#Preview {
    SynchroRulesPreferencesDetailView(repository: ExclusionRepository(), item: .apps)
}
