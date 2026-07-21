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

import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import kDriveResources
import SwiftUI

struct AdvancedPreferencesDebugEnableView: View {
    @State private var enableDebugLogs = false
    @State private var isShowingOpenURLError = false

    let repository: PreferencesRepository

    var body: some View {
        Section {
            ToggleView(
                title: KDriveLocalizable.enableDebugLogsSetting,
                description: KDriveLocalizable.enableDebugLogDescription,
                helperText: nil,
                isOn: $enableDebugLogs
            )
            .onChange(of: enableDebugLogs) { _ in
                @InjectService var matomo: MatomoUtils
                matomo.track(eventWithCategory: .advancedSettingsPage, name: "changeLogIsOn")
            }
        } header: {
            AdvancedPreferencesDebugHeaderView()
        } footer: {
            Button(KDriveLocalizable.buttonOpenDebugFolder, action: openDebugFolder)
        }
        .onAppear {
            enableDebugLogs = repository.parametersInfo.shouldUseLog
        }
        .onChange(of: enableDebugLogs) { newValue in
            updateRepositoryValue(\.$enableDebugLogs, \.shouldUseLog, newValue: newValue, repository: repository)
        }
        .alert(KDriveLocalizable.unexpectedErrorTeachingTipTitle, isPresented: $isShowingOpenURLError) {} message: {
            Text(KDriveLocalizable.errorOpeningLocalURL(generateDebugFolderURL()))
        }
    }

    private func generateDebugFolderURL() -> URL {
        return FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent(
            "Library/Logs/kDrive",
            isDirectory: true
        )
    }

    private func openDebugFolder() {
        @InjectService var matomo: MatomoUtils
        matomo.track(eventWithCategory: .advancedSettingsPage, name: "openLogFolder")
        let debugURL = generateDebugFolderURL()
        guard FileManager.default.fileExists(atPath: debugURL.path) else {
            isShowingOpenURLError = true
            return
        }

        NSWorkspace.shared.open(debugURL)
    }
}

#Preview {
    AdvancedPreferencesDebugEnableView(repository: PreferencesRepository())
}
