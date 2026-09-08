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

import Combine
import InfomaniakDI
import kDriveCore
import SwiftUI

@MainActor
final class SynchroErrorManager: ObservableObject {
    @Published var isShowingActivateOfflineSynchroSheet: SynchroError?
    @Published var isShowingLocalAccessSheet: SynchroError?
    @Published var isShowingResolutionTipsSheet: ExplanationsSheetType?

    enum ExplanationsSheetType: Identifiable, Sendable {
        var id: String {
            switch self {
            case .invalidSyncDirAccess(let synchroError):
                return "invalidSyncDirAccess_\(synchroError.metadata.dbId)"
            case .systemSyncDirAccess(let synchroError):
                return "systemSyncDirAccess_\(synchroError.metadata.dbId)"
            case .systemSyncDirDiskMissing:
                return "systemSyncDirDiskMissing"
            case .dataSyncDirChanged:
                return "dataSyncDirChanged"
            }
        }

        case invalidSyncDirAccess(SynchroError)
        case systemSyncDirAccess(SynchroError)
        case systemSyncDirDiskMissing
        case dataSyncDirChanged
    }

    // MARK: - Manage sync

    func refreshErrors(_ error: SynchroError) async {
        _ = try? await ErrorJobs().refreshSyncErrors(syncDbId: Int32(error.metadata.synchroDbId))
    }

    func tryToRestartSynchro(_ error: SynchroError) async {
        try? await SyncJobs().startSync(syncDbId: Int32(error.metadata.synchroDbId))
    }

    // MARK: - Open URLs

    func openFolder(_ error: SynchroError) {
        let url = URL(fileURLWithPath: error.metadata.path)
        NSWorkspace.shared.open(url)
    }

    func openParentFolder(_ error: SynchroError) {
        let url = URL(fileURLWithPath: error.metadata.path)
        let parentURL = url.deletingLastPathComponent()
        NSWorkspace.shared.open(parentURL)
    }

    func openShopURL(_ error: SynchroError) async {
        guard let drive = await getDrive(error) else {
            return
        }

        @InjectService var nodeURLGenerator: NodeURLGenerator
        let shopURL = nodeURLGenerator.shopURL(forDriveId: Int(drive.driveId))
        NSWorkspace.shared.open(shopURL)
    }

    func openSupportURL() {
        NSWorkspace.shared.open(URLConstants.help)
    }

    func openItemRemotely(_ error: SynchroError) async {
        guard let remoteNodeId = error.metadata.nodeId.remote, let drive = await getDrive(error) else {
            return
        }

        @InjectService var nodeURLGenerator: NodeURLGenerator
        let remoteURL = nodeURLGenerator.remoteURL(for: remoteNodeId, driveId: Int(drive.driveId))
        NSWorkspace.shared.open(remoteURL)
    }

    func openPreferencesSystemStorage() {
        NSWorkspace.shared.open(SystemPreferencesURL.storage)
    }

    func openPreferencesSystemLiteSync() {
        NSWorkspace.shared.open(SystemPreferencesURL.endpointSecurityExtension)
    }

    // MARK: - App navigation

    func navigateToLoginPage() {
        @InjectService var router: MainWindowRouter
        router.navigate(to: .onboarding())
    }

    func navigateToSynchroCreation() {
        (NSApp.delegate as? AppDelegate)?.openPreferencesWindow()

        @InjectService var router: PreferencesViewRouter
        router.setCurrentTab(.accounts)
    }

    func navigateToExclusionRules() {
        (NSApp.delegate as? AppDelegate)?.openPreferencesWindow()

        @InjectService var router: PreferencesViewRouter
        router.setCurrentTab(.advanced)
        router.append(.synchroRules)
    }

    // MARK: - Sheets

    func showActivateOfflineSynchroSheet(_ error: SynchroError) {
        isShowingActivateOfflineSynchroSheet = error
    }

    func showLocalAccessSheet(_ error: SynchroError) {
        isShowingLocalAccessSheet = error
    }

    func showResolutionTipsSheet(_ error: SynchroError) {
        switch error.kind {
        case .invalidSyncDirAccess:
            isShowingResolutionTipsSheet = .invalidSyncDirAccess(error)
        case .systemSyncDirAccess:
            isShowingResolutionTipsSheet = .systemSyncDirAccess(error)
        case .systemSyncDirDiskMissing:
            isShowingResolutionTipsSheet = .systemSyncDirDiskMissing
        case .dataSyncDirChanged:
            isShowingResolutionTipsSheet = .dataSyncDirChanged
        default:
            break
        }
    }

    // MARK: - Misc

    func handleConflict() {
        // TODO: Will be done in a next PR
    }

    func renameItem(_ error: SynchroError) async {
        if error.metadata.nodeId.remote != nil {
            await openItemRemotely(error)
        } else {
            openFolder(error)
        }
    }

    func closeApp() async {
        try? await UtilityJobs().quit()
        NSApp.terminate(nil)
    }

    // MARK: - Helpers

    private func getDrive(_ error: SynchroError) async -> Drive? {
        @InjectService var cache: CoherentCache
        guard let synchroContext = await cache.getSynchroContext(Int32(error.metadata.synchroDbId)) else {
            return nil
        }

        return synchroContext.drive
    }
}
