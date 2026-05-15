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

struct SynchroRulesPreferencesUserTemplateList: View {
    let repository: ExclusionRepository
    @Binding var userExcludedTemplates: [UIExclusionTemplateInfo]

    @State var selectedTemplates = Set<UIExclusionTemplateInfo.ID>()
    @State var isShowingAlert = false
    var body: some View {
        VStack(alignment: .leading) {
            TemplateTable(list: $userExcludedTemplates, selectedRows: $selectedTemplates, isShowingAlert: $isShowingAlert)

            HStack {
                Button(
                    "Supprimer \(selectedTemplates.count) élément\(selectedTemplates.count > 1 ? "s" : "")",
                    role: .destructive
                ) {
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
                            print("Error \(error.localizedDescription)")
                            userExcludedTemplates = oldExcludedTemplates
                            selectedTemplates = oldSelectedTemplates
                        }
                    }
                }
                .buttonStyle(.bordered)
                .controlSize(.regular)

                Button(KDriveLocalizable.buttonCancel) {
                    selectedTemplates.removeAll()
                }
                .buttonStyle(.borderless)
            }
            .opacity(selectedTemplates.isEmpty ? 0 : 1)
        }
        .alert(KDriveLocalizable.fileExclusionNotificationWarning, isPresented: $isShowingAlert) {}
    }
}

#Preview {
    SynchroRulesPreferencesUserTemplateList(repository: ExclusionRepository(), userExcludedTemplates: .constant([]))
}
