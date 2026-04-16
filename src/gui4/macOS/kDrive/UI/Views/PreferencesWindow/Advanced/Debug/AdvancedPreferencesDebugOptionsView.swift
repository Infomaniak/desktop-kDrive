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
    let labelTitle: String
    let labelDescription: String
    var body: some View {
        VStack(alignment: .leading) {
            Text(labelTitle)
            Text(labelDescription)
                .font(.Tokens.subheadline)
                .foregroundStyle(.secondary)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }
}

struct ToggleView: View {
    let labelTitle: String
    let labelDescription: String
    let helperText: String?

    @Binding var isOn: Bool

    var body: some View {
        HStack(alignment: .center) {
            LabelContainerView(labelTitle: labelTitle, labelDescription: labelDescription)
            Toggle("", isOn: $isOn).labelsHidden()
                .padding(.trailing, AppPadding.padding8)
            if let helperText {
                Image(systemName: "info.circle")
                    .resizable()
                    .frame(width: 16, height: 16)
                    .foregroundStyle(.secondary)
                    .help(helperText)
            }
        }
    }
}

struct AdvancedPreferencesDebugOptionsView: View {
    let repository: PreferencesRepository
    @State private var automaticCleaning = false
    @State private var extendedLog = false
    @State private var debugLevel: UILogLevel = .debug
    var body: some View {
        Section {
            ToggleView(
                labelTitle: KDriveLocalizable.autoCleanupLogsSetting,
                labelDescription: KDriveLocalizable.autoCleanupLogsDescription,
                helperText: nil,
                isOn: $automaticCleaning
            )
            ToggleView(
                labelTitle: KDriveLocalizable.extendedLogSetting,
                labelDescription: KDriveLocalizable.extendedLogDescription,
                helperText: KDriveLocalizable.extendedLogWarning,
                isOn: $extendedLog
            )
            HStack {
                LabelContainerView(
                    labelTitle: KDriveLocalizable.debugLevelSetting,
                    labelDescription: KDriveLocalizable.debugLevelDescription
                )
                Picker("", selection: $debugLevel) {
                    ForEach(UILogLevel.allCases, id: \.id) { level in
                        Text(level.label).tag(level)
                    }
                }
                .disabled(extendedLog)
                .labelsHidden()
                .pickerStyle(.menu)
            }
        }
        .onAppear(perform: getAllValues)
        .onChange(of: automaticCleaning, perform: { newValue in
            updateValue(\.$automaticCleaning, \.shouldPurgeOldLogs, newValue: newValue)
        })
        .onChange(of: extendedLog, perform: { newValue in
            updateValue(\.$extendedLog, \.isExtendedLogEnabled, newValue: newValue)
        })
        .onChange(of: debugLevel, perform: { newValue in
            updateValue(\.$debugLevel, \.logLevel, newValue: newValue)
        })
    }

    func getAllValues() {
        automaticCleaning = repository.parametersInfo.shouldPurgeOldLogs
        extendedLog = repository.parametersInfo.isExtendedLogEnabled
        debugLevel = repository.parametersInfo.logLevel
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
}

#Preview {
    AdvancedPreferencesDebugOptionsView(repository: PreferencesRepository())
}
