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

struct GeneralPreferencesMiscSection: View {
    @ObservedObject var repository: PreferencesRepository

    @State private var notificationsState: UINotificationState = .never
    @State private var launchOnStartup = true
    @State private var moveDeletedFilesToTrash = true

    var body: some View {
        Section {
            OptionPicker(
                KDriveLocalizable.labelNotifications,
                options: UINotificationState.allCases,
                selection: $notificationsState
            )
            .onChange(of: notificationsState) { newValue in
                updateValue(\.$notificationsState, \.notificationsState, newValue: newValue)
            }

            Toggle(KDriveLocalizable.openKDriveAtStartupSetting, isOn: $launchOnStartup)
                .onChange(of: launchOnStartup) { newValue in
                    updateValue(\.$launchOnStartup, \.launchOnStartup, newValue: newValue)
                }

            HStack {
                VStack(alignment: .leading) {
                    Text(KDriveLocalizable.moveDeletedFilesToRecycleBinSetting)
                    Text(KDriveLocalizable.moveDeletedFilesToRecycleBinWarning)
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
                .frame(maxWidth: .infinity, alignment: .leading)

                Toggle(KDriveLocalizable.moveDeletedFilesToRecycleBinSetting, isOn: $moveDeletedFilesToTrash)
                    .labelsHidden()
            }
            .onChange(of: moveDeletedFilesToTrash) { newValue in
                updateValue(\.$moveDeletedFilesToTrash, \.moveDeletedFilesToTrash, newValue: newValue)
            }
        }
        .onAppear {
            updatePropertiesFromParametersInfo(repository.parametersInfo)
        }
        .onChange(of: repository.parametersInfo) { newValue in
            updatePropertiesFromParametersInfo(newValue)
        }
    }

    private func updateValue<T: Equatable>(
        _ stateKeyPath: KeyPath<Self, Binding<T>>,
        _ repositoryKeyPath: WritableKeyPath<UIParametersInfo, T>,
        newValue: T
    ) {
        Task {
            guard newValue != repository.parametersInfo[keyPath: repositoryKeyPath] else { return }

            do {
                try await repository.update(repositoryKeyPath, value: newValue)
            } catch {
                self[keyPath: stateKeyPath].wrappedValue = repository.parametersInfo[keyPath: repositoryKeyPath]
            }
        }
    }

    private func updatePropertiesFromParametersInfo(_ parametersInfo: UIParametersInfo) {
        notificationsState = parametersInfo.notificationsState
        launchOnStartup = parametersInfo.launchOnStartup
        moveDeletedFilesToTrash = parametersInfo.moveDeletedFilesToTrash
    }
}

#Preview {
    GeneralPreferencesMiscSection(repository: PreferencesRepository())
}
