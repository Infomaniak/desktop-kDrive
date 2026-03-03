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
                Task {
                    guard newValue != preferencesRepository.parametersInfo.language else { return }
                    do {
                        try await preferencesRepository.update(\.language, value: newValue)
                    } catch {
                        language = preferencesRepository.parametersInfo.language
                    }
                }
            }

            OptionPicker(
                KDriveLocalizable.labelNotifications,
                options: UINotificationState.allCases,
                selection: $notificationsState
            )
            .onChange(of: notificationsState) { newValue in
                Task {
                    guard newValue != preferencesRepository.parametersInfo.notificationsState else { return }
                    do {
                        try await preferencesRepository.update(\.notificationsState, value: newValue)
                    } catch {
                        notificationsState = preferencesRepository.parametersInfo.notificationsState
                    }
                }
            }

            Toggle(KDriveLocalizable.openKDriveAtStartupSetting, isOn: $launchOnStartup)
                .onChange(of: launchOnStartup) { newValue in
                    Task {
                        guard newValue != preferencesRepository.parametersInfo.launchOnStartup else { return }
                        do {
                            try await preferencesRepository.update(\.launchOnStartup, value: newValue)
                        } catch {
                            launchOnStartup = preferencesRepository.parametersInfo.launchOnStartup
                        }
                    }
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
                Task {
                    guard newValue != preferencesRepository.parametersInfo.moveDeletedFilesToTrash else { return }
                    do {
                        try await preferencesRepository.update(\.moveDeletedFilesToTrash, value: newValue)
                    } catch {
                        moveDeletedFilesToTrash = preferencesRepository.parametersInfo.moveDeletedFilesToTrash
                    }
                }
            }
        }
        .onAppear {
            let parametersInfo = preferencesRepository.parametersInfo

            language = parametersInfo.language
            notificationsState = parametersInfo.notificationsState
            launchOnStartup = parametersInfo.launchOnStartup
            moveDeletedFilesToTrash = parametersInfo.moveDeletedFilesToTrash
        }
    }
}

#Preview {
    GeneralPreferencesMiscSection(preferencesRepository: PreferencesRepository())
}
