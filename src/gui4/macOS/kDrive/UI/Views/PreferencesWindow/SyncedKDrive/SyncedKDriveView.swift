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
import OrderedCollections
import SwiftUI

enum UISynchroMode: String, CaseIterable, Identifiable, Sendable {
    case storeOnline
    case availableOffline

    var id: String {
        return rawValue
    }

    var icon: Image {
        switch self {
        case .storeOnline:
            return KDriveResources.cloud.swiftUIImage
        case .availableOffline:
            return KDriveResources.computer.swiftUIImage
        }
    }

    var title: String {
        switch self {
        case .storeOnline:
            return KDriveLocalizable.storedOnline
        case .availableOffline:
            return KDriveLocalizable.availableOffline
        }
    }

    var description: String {
        switch self {
        case .storeOnline:
            return KDriveLocalizable.storedOnlineDescription
        case .availableOffline:
            return KDriveLocalizable.availableOfflineDescription
        }
    }
}

struct SyncedKDriveView: View {
    let drive: UIDrive

    @State private var isLoading = false

    @State private var mainSynchro: UISynchro?
    @State private var mainSynchroMode = UISynchroMode.storeOnline
    @State private var advancedSynchros = [UISynchro]()

    var body: some View {
        Form {
            Section {
                // Empty on purpose
            } header: {
                HStack {
                    BadgeView(
                        image: KDriveResources.kdriveFoldersStacked.swiftUIImage,
                        color: drive.color ?? ColorToken.Drive.defaultColor.asColor,
                        iconSize: 16,
                        squareSize: 26,
                        radius: AppRadius.radius8
                    )
                    Text(drive.name)
                }
            }

            if let mainSynchro {
                Section {
                    IKLabeledContent(KDriveLocalizable.labelSyncLocation) {
                        Button(mainSynchro.localPath.path) {
                            NSWorkspace.shared.open(mainSynchro.localPath)
                        }
                        .buttonStyle(.plain)
                        .foregroundStyle(ColorToken.Action.primary.asColor)
                    }

                    IKLabeledContent(KDriveLocalizable.labelSynchronisation) {
                        Button(KDriveLocalizable.buttonManage) {
                            // TODO: Navigate to synchro management
                        }
                        .disabled(true)
                    }
                }

                Section {
                    VStack(alignment: .leading, spacing: AppPadding.padding16) {
                        VStack(alignment: .leading, spacing: AppPadding.padding2) {
                            Text(KDriveLocalizable.fileSyncMode)
                            Text(KDriveLocalizable.fileSyncModeDescription)
                                .foregroundStyle(.secondary)
                                .font(.callout)
                        }

                        Picker("", selection: $mainSynchroMode) {
                            ForEach(UISynchroMode.allCases) { mode in
                                HStack(alignment: .top) {
                                    BadgeView(image: mode.icon, color: ColorToken.Action.primary.asColor)

                                    VStack(alignment: .leading) {
                                        Text(mode.title)
                                        Text(mode.description)
                                            .foregroundStyle(.secondary)
                                            .font(.callout)
                                    }
                                }
                                .tag(mode)
                            }
                        }
                        .pickerStyle(.radioGroup)
                        .labelsHidden()
                    }
                }

                Section {
                    Button(role: .destructive, action: removePrincipalSynchro) {
                        Text(KDriveLocalizable.buttonRemoveSync)
                            .foregroundStyle(.red)
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .contentShape(.rect)
                    }
                    .buttonStyle(.plain)
                }
            }

            if !advancedSynchros.isEmpty {
                Section {
                    Button(action: navigateToAdvancedSynchro) {
                        HStack {
                            Text(KDriveLocalizable.advancedSyncTitle)
                                .frame(maxWidth: .infinity, alignment: .leading)
                                .foregroundStyle(.primary)

                            Image(systemName: "chevron.right")
                                .foregroundStyle(.secondary)
                        }
                        .contentShape(.rect)
                    }
                    .buttonStyle(.plain)
                }
            }
        }
        .groupedFormatStyle()
        .task {
            await fetchSynchros()
        }
    }

    private func fetchSynchros() async {
        withAnimation {
            isLoading = true
        }

        @InjectService var coherentCache: CoherentCache
        guard let driveSynchros = await coherentCache.getDrive(driveDbId: Int32(drive.dbId))?.synchros.values else {
            return
        }

        var freshMainSynchro: UISynchro?
        var freshAdvancedSynchros = [UISynchro]()

        for synchro in driveSynchros {
            let uiSynchro = UISynchro(synchro: synchro)

            if uiSynchro.targetNodeId == nil, freshMainSynchro == nil {
                freshMainSynchro = uiSynchro
            } else {
                freshAdvancedSynchros.append(uiSynchro)
            }
        }

        withAnimation {
            isLoading = false

            mainSynchro = freshMainSynchro
            mainSynchroMode = freshMainSynchro?.useVirtualFileSystem == true ? .storeOnline : .availableOffline

            advancedSynchros = freshAdvancedSynchros
        }
    }

    private func removePrincipalSynchro() {}

    private func navigateToAdvancedSynchro() {
        // TODO: Navigate to advanced synchros
    }
}

#Preview {
    SyncedKDriveView(drive: PreviewHelper.drive1)
}
