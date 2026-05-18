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

struct SynchroRulesPreferencesUserAppList: View {
    let repository: ExclusionRepository
    @Binding var userExcludedApps: [UIExclusionAppInfo]

    @State var selectedApps = Set<UIExclusionAppInfo.ID>()

    var body: some View {
        VStack(alignment: .leading) {
            Table(userExcludedApps) {
                TableColumn("") { item in
                    Toggle("", isOn: Binding(
                        get: { selectedApps.contains(item.id) },
                        set: { isSelected in
                            if isSelected {
                                selectedApps.insert(item.id)
                            } else {
                                selectedApps.remove(item.id)
                            }
                        }
                    ))
                    .toggleStyle(.checkbox)
                    .labelsHidden()
                }
                .width(24)

                TableColumn(KDriveLocalizable.labelName) { item in
                    Text(item.app)
                        .frame(maxWidth: .infinity, alignment: .leading)
                }
            }

            HStack {
                Button(KDriveLocalizable.buttonRemoveFileExclusionRule(selectedApps.count),
                       role: .destructive) {
                    let newExcludedApps: [UIExclusionAppInfo] = userExcludedApps.filter {
                        !selectedApps.contains($0.id)
                    }
                    Task {
                        do {
                            try await repository.updateApps(updatedApps: newExcludedApps)

                            userExcludedApps = newExcludedApps
                            selectedApps.removeAll()
                        } catch {
                            print("Error \(error.localizedDescription)")
                        }
                    }
                }
                .foregroundStyle(.red)

                Button(KDriveLocalizable.buttonCancel) {
                    selectedApps.removeAll()
                }
            }
            .opacity(selectedApps.isEmpty ? 0 : 1)
        }
    }
}

#Preview {
    SynchroRulesPreferencesUserAppList(repository: ExclusionRepository(), userExcludedApps: .constant([]))
}
