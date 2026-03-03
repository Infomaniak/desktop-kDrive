/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2025 Infomaniak Network SA

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

protocol PreferenceOption: Hashable, Identifiable {
    var label: String { get }
}

extension UIAppLanguage: PreferenceOption {
    var label: String {
        switch self {
        case .system:
            return KDriveLocalizable.labelSameAsSystem
        case .french:
            return "Français"
        case .english:
            return "English"
        case .german:
            return "Deutsch"
        case .spanish:
            return "Español"
        case .italian:
            return "Italiano"
        }
    }
}

extension UINotificationState: PreferenceOption {
    var label: String {
        switch self {
        case .always:
            return KDriveLocalizable.notificationsDisabledAlways
        case .never:
            return KDriveLocalizable.notificationsDisabledNever
        case .forOneHour:
            return KDriveLocalizable.forOneHour
        case .untilTomorrow:
            return KDriveLocalizable.untilTomorrow
        case .forThreeDays:
            return KDriveLocalizable.forThreeDays
        case .forOneWeek:
            return KDriveLocalizable.forOneWeek
        }
    }
}

struct GeneralPreferencesMiscSection: View {
    @ObservedObject var preferencesRepository: PreferencesRepository

    @State private var language: UIAppLanguage = .english
    @State private var notificationsState: UINotificationState = .never
    @State private var launchOnStartup = true
    @State private var moveDeletedFilesToTrash = true

    var body: some View {
        Section {
            OptionPicker(
                KDriveLocalizable.languageSetting,
                options: UIAppLanguage.allCases,
                selection: $language
            )
            .onChange(of: language) { newValue in
                updateValue(\.$language, \.language, newValue: newValue)
            }

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
            updatePropertiesFromParametersInfo(preferencesRepository.parametersInfo)
        }
        .onChange(of: preferencesRepository.parametersInfo) { newValue in
            updatePropertiesFromParametersInfo(newValue)
        }
    }

    private func updateValue<T: Equatable>(
        _ state: KeyPath<Self, Binding<T>>,
        _ repository: WritableKeyPath<UIParametersInfo, T>,
        newValue: T
    ) {
        Task {
            guard newValue != preferencesRepository.parametersInfo[keyPath: repository] else { return }

            do {
                try await preferencesRepository.update(repository, value: newValue)
            } catch {
                self[keyPath: state].wrappedValue = preferencesRepository.parametersInfo[keyPath: repository]
            }
        }
    }

    private func updatePropertiesFromParametersInfo(_ parametersInfo: UIParametersInfo) {
        language = parametersInfo.language
        notificationsState = parametersInfo.notificationsState
        launchOnStartup = parametersInfo.launchOnStartup
        moveDeletedFilesToTrash = parametersInfo.moveDeletedFilesToTrash
    }
}

#Preview {
    GeneralPreferencesMiscSection(preferencesRepository: PreferencesRepository())
}
