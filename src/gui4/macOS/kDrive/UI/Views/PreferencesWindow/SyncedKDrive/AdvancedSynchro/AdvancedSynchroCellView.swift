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
import Sentry
import SwiftUI

struct AdvancedSynchroCellView: View {
    @State private var synchroMode: UISynchroMode
    @State private var blacklistNodes: Set<String>?
    @State private var isShowingGenericError = false

    let synchro: UISynchro
    let userDbId: UIUser.ID?
    let driveId: UIDrive.ID?
    let onDelete: () -> Void

    init(synchro: UISynchro, userDbId: Int?, driveId: Int?, onDelete: @escaping () -> Void) {
        self.synchro = synchro
        self.userDbId = userDbId
        self.driveId = driveId
        self.onDelete = onDelete

        _synchroMode = State(initialValue: synchro.useVirtualFileSystem ? .storeOnline : .availableOffline)
    }

    var body: some View {
        Section {
            HStack(spacing: AppPadding.padding8) {
                BadgeView(image: KDriveResources.folder.swiftUIImage, color: .accentColor)

                Text(synchro.localPath.lastPathComponent)
                    .font(.Tokens.bodyEmphasized)
                    .frame(maxWidth: .infinity, alignment: .leading)

                Menu {
                    Button(action: openInFinder) {
                        Label(KDriveLocalizable.buttonOpenInFinder, resource: KDriveResources.finder)
                    }
                    Button(action: openInBrowser) {
                        Label(KDriveLocalizable.buttonOpenInBrowser, resource: KDriveResources.squareArrowDiagonalUp)
                    }
                    Divider()
                    Button(role: .destructive, action: onDelete) {
                        Label(KDriveLocalizable.buttonRemoveSync, resource: KDriveResources.trash)
                    }
                } label: {
                    Image(systemName: "ellipsis")
                }
                .menuStyle(.borderlessButton)
                .menuIndicator(.hidden)
            }
            .task {
                await fetchBlacklistedNodes()
            }
            .genericErrorAlert(isPresented: $isShowingGenericError)

            IKLabeledContent(KDriveLocalizable.labelSyncLocation) {
                Button(synchro.localPath.path) {
                    NSWorkspace.shared.open(synchro.localPath)
                }
                .buttonStyle(.plain)
                .foregroundStyle(ColorToken.Action.primary.asColor)
            }

            IKLabeledContent("Dossiers synchronisés") {
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

            SynchroModePicker(synchroMode: $synchroMode)
                .disabled(!synchro.supportsVirtualFileSystem)
                .onChange(of: synchroMode) { newValue in
                    switchSynchroMode(newValue)
                }
        }
    }

    private func fetchBlacklistedNodes() async {
        do {
            let blacklistedNodes = try await BlacklistJobs().getBlacklistedNodeList(syncDbId: Int32(synchro.dbId))
            blacklistNodes = Set(blacklistedNodes)
        } catch {
            SentrySDK.capture(error: error)
        }
    }

    private func openInFinder() {
        NSWorkspace.shared.open(synchro.localPath)
    }

    private func openInBrowser() {
        Task {
            @InjectService var cache: CoherentCache
            guard let cache = await cache.getDrive(driveDbId: Int32(synchro.driveDbId)),
                  let targetNodeId = synchro.targetNodeId else {
                return
            }

            @InjectService var nodeURLGenerator: NodeURLGenerator
            let url = nodeURLGenerator.remoteURL(for: targetNodeId, driveId: Int(cache.driveId))

            NSWorkspace.shared.open(url)
        }
    }

    private func navigateToManageSynchro() {
        guard let userDbId, let driveId else {
            return
        }

        @InjectService var router: PreferencesViewRouter
        router.append(.blacklist(userDbId, driveId, synchro.dbId, rootNodeId: synchro.targetNodeId))
    }

    private func switchSynchroMode(_ mode: UISynchroMode) {
        Task {
            do {
                try await SyncJobs().setSupportsVirtualFiles(syncDbId: Int32(synchro.dbId), value: mode == .storeOnline)
            } catch {
                isShowingGenericError = true
            }
        }
    }
}

#Preview {
    Form {
        AdvancedSynchroCellView(
            synchro: UISynchro(
                dbId: 1,
                driveDbId: 1,
                localPath: URL(fileURLWithPath: "/Users/john/Téléchargements"),
                targetNodeId: "42",
                supportsVirtualFileSystem: true,
                useVirtualFileSystem: true
            ),
            userDbId: 1,
            driveId: 1
        ) { /* Empty on purpose */ }
    }
    .groupedFormatStyle()
}
