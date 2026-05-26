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

struct SynchroRulesPreferencesUserTemplateList: View {
    let repository: ExclusionRepository
    @Binding var userExcludedTemplates: [UIExclusionTemplateInfo]

    @State var selectedTemplates = Set<UIExclusionTemplateInfo.ID>()
    var body: some View {
        VStack(alignment: .leading) {
            Table(userExcludedTemplates) {
                TableColumn("") { item in
                    Toggle("", isOn: Binding(
                        get: { selectedTemplates.contains(item.id) },
                        set: { isSelected in
                            if isSelected {
                                selectedTemplates.insert(item.id)
                            } else {
                                selectedTemplates.remove(item.id)
                            }
                        }
                    ))
                    .toggleStyle(.checkbox)
                    .labelsHidden()
                }
                .width(24)

                TableColumn(KDriveLocalizable.labelRules) { item in
                    Text(item.displayName)
                        .frame(maxWidth: .infinity, alignment: .leading)
                }

                TableColumn(KDriveLocalizable.labelNotifyIfFileExcluded) { item in
                    Toggle("", isOn: Binding(
                        get: { item.warning },
                        set: { isSelected in
                            let index = userExcludedTemplates.firstIndex { $0.id == item.id }!
                            userExcludedTemplates[index].warning = isSelected
                            updateToggleInRepository()
                        }
                    ))
                    .toggleStyle(.switch)
                    .controlSize(.small)
                }
            }

            HStack {
                Button(KDriveLocalizable.buttonRemoveFileExclusionRule(selectedTemplates.count),
                       role: .destructive) {
                    let oldExcludedTemplates = userExcludedTemplates
                    let oldSelectedTemplates = selectedTemplates

                    userExcludedTemplates = userExcludedTemplates.filter {
                        !selectedTemplates.contains($0.id)
                    }
                    selectedTemplates.removeAll()

                    Task {
                        do {
                            try await repository.updateTemplates(updatedTemplates: userExcludedTemplates)
                        } catch {
                            userExcludedTemplates = oldExcludedTemplates
                            selectedTemplates = oldSelectedTemplates
                        }
                    }
                }
                .foregroundStyle(.red)

                Button(KDriveLocalizable.buttonCancel) {
                    selectedTemplates.removeAll()
                }
                .buttonStyle(.borderless)
            }
            .opacity(selectedTemplates.isEmpty ? 0 : 1)
        }
    }

    func updateToggleInRepository() {
        Task {
            do {
                try await repository.updateTemplates(updatedTemplates: userExcludedTemplates)
            } catch {
                userExcludedTemplates = repository.exclusionInfo.userExcludedTemplates
            }
        }
    }
}

#Preview {
    SynchroRulesPreferencesUserTemplateList(repository: ExclusionRepository(), userExcludedTemplates: .constant([]))
}
