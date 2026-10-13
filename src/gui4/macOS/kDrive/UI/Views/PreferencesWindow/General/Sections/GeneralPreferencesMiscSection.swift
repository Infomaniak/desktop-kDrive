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

import InfomaniakDI
import kDriveCoreUI
import kDriveResources
import SwiftUI

struct GeneralPreferencesMiscSection: View {
    @InjectService private var matomo: MatomoUtils
    @ObservedObject var repository: PreferencesRepository

    @State private var areNotificationsEnabled = true
    @State private var launchOnStartup = true
    @State private var moveDeletedFilesToTrash = true

    var body: some View {
        Section {
            Toggle(KDriveLocalizable.labelNotifications, isOn: $areNotificationsEnabled)
                .onChange(of: areNotificationsEnabled) { newValue in
                    updateNotificationsState(areNotificationsEnabled: newValue)
                }

            Toggle(KDriveLocalizable.openKDriveAtStartupSetting, isOn: $launchOnStartup)
                .onChange(of: launchOnStartup) { newValue in
                    updateRepositoryValue(\.$launchOnStartup, \.launchOnStartup, newValue: newValue, repository: repository)

                    guard newValue != repository.parametersInfo.launchOnStartup else { return }
                    matomo.track(eventWithCategory: .generalSettingsPage, name: "changeAutoStart")
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
                updateRepositoryValue(
                    \.$moveDeletedFilesToTrash,
                    \.moveDeletedFilesToTrash,
                    newValue: newValue,
                    repository: repository
                )

                guard newValue != repository.parametersInfo.moveDeletedFilesToTrash else { return }
                matomo.track(eventWithCategory: .generalSettingsPage, name: "changeMoveToTrash")
            }
        }
        .onAppear {
            updatePropertiesFromParametersInfo(repository.parametersInfo)
        }
        .onChange(of: repository.parametersInfo) { newValue in
            updatePropertiesFromParametersInfo(newValue)
        }
    }

    private func updatePropertiesFromParametersInfo(_ parametersInfo: UIParametersInfo) {
        areNotificationsEnabled = parametersInfo.notificationsState != .always
        launchOnStartup = parametersInfo.launchOnStartup
        moveDeletedFilesToTrash = parametersInfo.moveDeletedFilesToTrash
    }

    private func updateNotificationsState(areNotificationsEnabled: Bool) {
        let newState: UINotificationState = areNotificationsEnabled ? .never : .always
        guard newState != repository.parametersInfo.notificationsState else { return }

        matomo.track(eventWithCategory: .generalSettingsPage, name: "changeNotifications")
        Task {
            do {
                try await repository.update(\.notificationsState, value: newState)
            } catch {
                self.areNotificationsEnabled = repository.parametersInfo.notificationsState != .always
            }
        }
    }
}

#Preview {
    GeneralPreferencesMiscSection(repository: PreferencesRepository())
}
