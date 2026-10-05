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

import CppInterop
import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import kDriveResources
import SwiftUI

struct LightSyncSelectionSection: View {
    @EnvironmentObject private var viewModel: SynchroConfigurationFlowViewModel

    @State private var useLightSync = true
    @State private var supportsLightSync = true

    let configuration: SynchroConfiguration

    var body: some View {
        Section {
            VStack(alignment: .leading, spacing: AppPadding.padding8) {
                Text(KDriveLocalizable.storedOnline)
                    .font(.Tokens.headline)

                HStack(spacing: AppPadding.padding16) {
                    Text(KDriveLocalizable.storedOnlineDescription)
                        .font(.Tokens.body)
                        .frame(maxWidth: .infinity, alignment: .leading)

                    Toggle(KDriveLocalizable.storedOnline, isOn: $useLightSync)
                        .labelsHidden()
                        .onAppear {
                            useLightSync = configuration.useLightSync
                        }
                        .onChange(of: useLightSync) { newValue in
                            @InjectService var matomo: MatomoUtils
                            matomo.track(
                                eventWithCategory: .driveSetupDialog,
                                name: "changeSyncMode",
                                value: useLightSync
                            )
                            viewModel.updateConfiguration(configuration.id, useLightSync: newValue)
                        }
                        .disabled(!supportsLightSync)
                }
            }
            .foregroundStyle(ColorToken.Text.primary.asColor)
            .task(id: configuration.localFolder.url) {
                guard let localFolder = configuration.localFolder.url,
                      let canUseLightSync = try? await SyncCreationService().canUseLightSync(at: localFolder)
                else { return }

                if canUseLightSync {
                    if !supportsLightSync {
                        useLightSync = true
                    }
                    supportsLightSync = true
                } else {
                    useLightSync = false
                    viewModel.updateConfiguration(configuration.id, useLightSync: false)
                }
            }
        }
    }
}

#Preview {
    LightSyncSelectionSection(configuration: .init(drive: PreviewHelper.drive1, blackList: [], useLightSync: true))
}
