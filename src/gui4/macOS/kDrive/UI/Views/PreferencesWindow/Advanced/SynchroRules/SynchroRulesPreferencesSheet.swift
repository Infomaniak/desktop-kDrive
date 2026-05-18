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

import kDriveCore
import kDriveCoreUI
import kDriveResources
import SwiftUI

struct SynchroRulesPreferencesSheet: View {
    @Environment(\.dismiss) private var dismiss

    let item: SynchroRulesItem
    let repository: ExclusionRepository

    @Binding var userExcludedApps: [UIExclusionAppInfo]
    @Binding var userExcludedTemplates: [UIExclusionTemplateInfo]

    @State private var appList: [String: String] = [:]
    @State private var appId = ""
    @State private var input = ""
    @State private var isNotified = false
    @State private var isCreatingExclusion = false

    private var canAddRule: Bool {
        if item == .files {
            return !input.isEmpty
        } else {
            return !appId.isEmpty && !input.isEmpty
        }
    }

    private var placeholderText: String {
        switch item {
        case .files:
            return KDriveLocalizable.filesToExclude
        case .apps:
            return KDriveLocalizable.appToExclude
        }
    }

    var body: some View {
        VStack(alignment: .leading) {
            Text(KDriveLocalizable.dialogNewExclusionRuleTitle)
                .font(.Tokens.headline)
                .foregroundStyle(ColorToken.Text.primary.asColor)
                .padding(.bottom, AppPadding.padding4)

            if item == .files {
                Text(KDriveLocalizable.excludeRuleDescription)
                    .font(.Tokens.callout)
                    .foregroundStyle(.secondary)
            }

            if item == .apps {
                Picker(KDriveLocalizable.labelAppID, selection: $appId) {
                    ForEach(Array(appList.keys), id: \.self) { key in
                        Text(key)
                    }
                }
                .frame(maxWidth: .infinity, alignment: .leading)
            }
            TextField(placeholderText, text: $input)

            if item == .files {
                Toggle(isOn: $isNotified) {
                    Text(KDriveLocalizable.notifyOnFileExcluded)
                }
            }
        }
        .padding()
        .toolbar {
            ToolbarItem(placement: .cancellationAction) {
                Button(KDriveLocalizable.buttonCancel, role: .cancel) {
                    dismiss()
                }
            }
            ToolbarItem(placement: .confirmationAction) {
                LoadingButton(isLoading: $isCreatingExclusion) {
                    switch item {
                    case .files:
                        saveTemplate()
                    case .apps:
                        saveApp()
                    }
                    dismiss()
                } label: {
                    Text(KDriveLocalizable.buttonAddFileExclusionRule)
                }
                .disabled(!canAddRule)
            }
        }
        .onAppear(perform: getAppList)
        .onChange(of: appId) { newValue in
            guard let appDescription = appList[newValue] else { return }
            input = appDescription
        }
    }

    func saveTemplate() {
        var templates = repository.exclusionInfo.userExcludedTemplates
        let newTemplate = UIExclusionTemplateInfo(template: input, warning: isNotified)
        templates.append(newTemplate)

        Task {
            do {
                userExcludedTemplates = templates
                try await repository.updateTemplates(updatedTemplates: templates)
            } catch {
                userExcludedTemplates = repository.exclusionInfo.userExcludedTemplates
            }
        }
        dismiss()
    }

    func saveApp() {
        var excludedApps = repository.exclusionInfo.userExcludedApps
        let newExcludedApp = UIExclusionAppInfo(app: appId, description: input)
        excludedApps.append(newExcludedApp)

        Task {
            do {
                userExcludedApps = excludedApps
                try await repository.updateApps(updatedApps: excludedApps)
            } catch {
                userExcludedApps = repository.exclusionInfo.userExcludedApps
            }
        }

        dismiss()
    }

    func getAppList() {
        Task {
            do {
                let userExcludedAppIdentifiers = userExcludedApps.map(\.app)
                let dict = try await ExclusionAppJobs().getFetchingAppList()
                    .filter { !userExcludedAppIdentifiers.contains($0.key) }

                self.appList = dict

                appId = self.appList.keys.first ?? ""

            } catch {
                print("Error while fetching app list: \(error)")
            }
        }
    }
}

#Preview {
    SynchroRulesPreferencesSheet(
        item: .apps,
        repository: ExclusionRepository(),
        userExcludedApps: .constant([]),
        userExcludedTemplates: .constant([])
    )
}
