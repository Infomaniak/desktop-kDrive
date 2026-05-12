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

import kDriveCoreUI
import kDriveResources
import SwiftUI

struct LabelContainerView: View {
    let title: String
    let description: String

    var body: some View {
        VStack(alignment: .leading) {
            Text(title)
                .font(.Tokens.body)

            Text(description)
                .font(.Tokens.subheadline)
                .foregroundStyle(.secondary)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }
}

struct ToggleView: View {
    let title: String
    let description: String
    let helperText: String?

    @State private var isShowingAlert = false

    @Binding var isOn: Bool

    var body: some View {
        HStack(alignment: .center, spacing: AppPadding.padding8) {
            LabelContainerView(title: title, description: description)

            Toggle(title, isOn: $isOn)
                .labelsHidden()

            if let helperText {
                InformationButton {
                    isShowingAlert = true
                }
                .alert(helperText, isPresented: $isShowingAlert) {}
            }
        }
    }
}

struct AdvancedPreferencesDebugOptionsView: View {
    @State private var automaticCleaning = false
    @State private var extendedLog = false
    @State private var debugLevel = UILogLevel.debug

    let repository: PreferencesRepository

    var body: some View {
        Section {
            ToggleView(
                title: KDriveLocalizable.autoCleanupLogsSetting,
                description: KDriveLocalizable.autoCleanupLogsDescription,
                helperText: nil,
                isOn: $automaticCleaning
            )

            ToggleView(
                title: KDriveLocalizable.extendedLogSetting,
                description: KDriveLocalizable.extendedLogDescription,
                helperText: KDriveLocalizable.extendedLogWarning,
                isOn: $extendedLog
            )

            HStack {
                LabelContainerView(
                    title: KDriveLocalizable.debugLevelSetting,
                    description: KDriveLocalizable.debugLevelDescription
                )

                Picker(KDriveLocalizable.debugLevelSetting, selection: $debugLevel) {
                    ForEach(UILogLevel.allCases, id: \.id) { level in
                        Text(level.label).tag(level)
                    }
                }
                .disabled(extendedLog)
                .labelsHidden()
                .pickerStyle(.menu)
            }
        }
        .onAppear(perform: getInitialValues)
        .onChange(of: automaticCleaning) { newValue in
            updateRepositoryValue(\.$automaticCleaning, \.shouldPurgeOldLogs, newValue: newValue, repository: repository)
        }
        .onChange(of: extendedLog) { newValue in
            updateRepositoryValue(\.$extendedLog, \.isExtendedLogEnabled, newValue: newValue, repository: repository)
        }
        .onChange(of: debugLevel) { newValue in
            updateRepositoryValue(\.$debugLevel, \.logLevel, newValue: newValue, repository: repository)
        }
    }

    func getInitialValues() {
        automaticCleaning = repository.parametersInfo.shouldPurgeOldLogs
        extendedLog = repository.parametersInfo.isExtendedLogEnabled
        debugLevel = repository.parametersInfo.logLevel
    }
}

#Preview {
    AdvancedPreferencesDebugOptionsView(repository: PreferencesRepository())
}
