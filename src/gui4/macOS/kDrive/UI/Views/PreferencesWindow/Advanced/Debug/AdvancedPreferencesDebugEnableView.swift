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

    let repository: PreferencesRepository

    var body: some View {
        Section {
            ToggleView(
                labelTitle: KDriveLocalizable.enableDebugLogsSetting,
                labelDescription: KDriveLocalizable.enableDebugLogDescription,
                helperText: nil,
                isOn: $enableDebugLogs
            )
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
    }

    private func openDebugFolder() {
        @InjectService var nodeURLGenerator: NodeURLGenerator
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent(
            "/kdrive-logdir",
            isDirectory: true
        )
        NSWorkspace.shared.open(directory)
    }
}

#Preview {
    AdvancedPreferencesDebugEnableView(repository: PreferencesRepository())
}
