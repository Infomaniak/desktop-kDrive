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
import kDriveCore
import kDriveCoreUI
import kDriveResources
import OrderedCollections
import Sentry
import SwiftUI

struct SyncedKDriveView: View {
    @InjectService private var matomo: MatomoUtils
    let drive: UIDrive

    @State private var mainSynchro: UISynchro?
    @State private var mainSynchroMode = UISynchroMode.storeOnline
    @State private var committedMainSynchroMode = UISynchroMode.storeOnline

    @State private var synchroToDelete: UISynchro?
    @State private var isShowingGenericError = false

    @State private var blacklistNodes: Set<String>?

    var body: some View {
        Form {
            Section { /* Empty on purpose */ } header: {
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
                            matomo.track(eventWithCategory: .driveManagementPage, name: "openSyncDir")
                            NSWorkspace.shared.open(mainSynchro.localPath)
                        }
                        .buttonStyle(.plain)
                        .foregroundStyle(ColorToken.Action.primary.asColor)
                    }

                    IKLabeledContent(KDriveLocalizable.labelSynchronisation) {
                        HStack {
                            if let blacklistNodes, !blacklistNodes.isEmpty {
                                Text(KDriveLocalizable.onboardingExclusionSummarySome)
                                    .font(.Tokens.body)
                                    .foregroundStyle(.secondary)
                            }

                            Button(KDriveLocalizable.buttonManage, action: navigateToManageSynchro)
                                .disabled(blacklistNodes == nil)
                        }
                    }
                }

                Section {
                    SynchroModePicker(synchroDbId: mainSynchro.dbId, synchroMode: $mainSynchroMode)
                        .disabled(!mainSynchro.supportsVirtualFileSystem)
                        .onChange(of: mainSynchroMode) { newValue in
                            guard newValue != committedMainSynchroMode else { return }
                            switchSynchroMode(mainSynchro, mode: newValue)
                        }
                }

                Section {
                    FormNavigationCell(title: KDriveLocalizable.advancedSyncTitle, navigate: navigateToAdvancedSynchro)
                }

                Section {
                    Button(role: .destructive) {
                        synchroToDelete = mainSynchro
                    } label: {
                        Text(KDriveLocalizable.buttonRemoveSync)
                            .foregroundStyle(.red)
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .contentShape(.rect)
                    }
                    .buttonStyle(.plain)
                }
            } else {
                Section {
                    HStack(spacing: AppPadding.padding8) {
                        Text(KDriveLocalizable.labelSynchronisation)
                            .foregroundStyle(.primary)
                            .frame(maxWidth: .infinity, alignment: .leading)

                        Text(KDriveLocalizable.notSyncedDrive)
                            .foregroundStyle(.secondary)
                        Button(KDriveLocalizable.buttonEnable, action: enableMainSynchro)
                            .buttonStyle(.bordered)
                    }
                }

                Section {
                    FormNavigationCell(title: KDriveLocalizable.advancedSyncTitle, navigate: navigateToAdvancedSynchro)
                }
            }
        }
        .groupedFormatStyle()
        .task {
            await fetchSynchros()
            await fetchBlacklistedNodes()
        }
        .sheet(item: $synchroToDelete) { synchro in
            RemoveSynchroConfirmationView(synchroDbId: synchro.dbId, completion: handleSynchroIsDeleted)
        }
        .genericErrorAlert(isPresented: $isShowingGenericError)
    }

    private func fetchSynchros() async {
        @InjectService var coherentCache: CoherentCache
        guard let driveSynchros = await coherentCache.getDrive(driveDbId: Int32(drive.dbId))?.synchros.values else {
            return
        }

        let freshMainSynchro = driveSynchros
            .map { UISynchro(synchro: $0) }
            .first { $0.targetNodeId == nil }

        let fetchedMode: UISynchroMode = freshMainSynchro?.useVirtualFileSystem == true ? .storeOnline : .availableOffline
        withAnimation {
            mainSynchro = freshMainSynchro
            mainSynchroMode = fetchedMode
        }
        committedMainSynchroMode = fetchedMode
    }

    private func fetchBlacklistedNodes() async {
        guard let mainSynchroId = mainSynchro?.dbId else {
            return
        }

        do {
            let blacklistedNodes = try await BlacklistJobs().getBlacklistedNodeList(syncDbId: Int32(mainSynchroId))
            blacklistNodes = Set(blacklistedNodes)
        } catch {
            SentrySDK.capture(error: error)
        }
    }

    private func navigateToManageSynchro() {
        matomo.track(eventWithCategory: .driveManagementPage, name: "showItemExclusion")
        Task {
            @InjectService var cache: CoherentCache
            guard let mainSynchro, let drive = await cache.getDrive(driveDbId: Int32(drive.dbId)) else {
                return
            }

            @InjectService var router: PreferencesViewRouter
            router.append(.blacklist(Int(drive.userDbId), Int(drive.driveId), Int(mainSynchro.dbId), rootNodeId: nil))
        }
    }

    private func switchSynchroMode(_ synchro: UISynchro, mode: UISynchroMode) {
        Task {
            do {
                try await SyncJobs().setSupportsVirtualFiles(syncDbId: Int32(synchro.dbId), value: mode == .storeOnline)
                committedMainSynchroMode = mode
            } catch {
                mainSynchroMode = committedMainSynchroMode
                isShowingGenericError = true
            }
        }
    }

    private func navigateToAdvancedSynchro() {
        matomo.track(eventWithCategory: .driveManagementPage, name: "manageAdvancedSync")
        @InjectService var router: PreferencesViewRouter
        router.append(.advancedSynchros(drive))
    }

    private func handleSynchroIsDeleted(_ error: Error?) {
        guard error == nil else {
            isShowingGenericError = true
            return
        }

        @InjectService var router: PreferencesViewRouter
        router.append(.accounts)
    }

    private func enableMainSynchro() {
        // TODO: Add Matomo here
        // TODO: Add logic here
    }
}

#Preview {
    SyncedKDriveView(drive: PreviewHelper.drive1)
}
