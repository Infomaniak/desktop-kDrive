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

import Foundation
import InfomaniakDI
import Sentry

public protocol XPCSignalHandlerProtocol {
    func handleServerSignal(_ signal: Data?)
}

struct XPCSignalHandler: XPCSignalHandlerProtocol {
    let decoder = JSONDecoder()
    @LazyInjectService var coherentCache: CoherentCache

    enum SignalError: Error {
        case nilData
        case unableToParseMetadata(_ signal: String)
        case serverError(_ code: KDC.ExitCode?, _ cause: KDC.ExitCause?)
        case unableToGetUserFromSignal
        case unableToGetUserDbIdFromSignal
        case unableToGetAccountFromSignal
        case unableToGetAccountDbIdFromSignal
        case unableToGetDriveFromSignal
        case unableToGetDriveDbIdFromSignal
        case unableToGetSyncFromSignal
        case unableToGetSyncDbIdFromSignal
        case unableToGetSyncProgressFromSignal
        case unableToGetSyncFileItemFromSignal
        case unsupported(_ num: SignalNum)
    }

    func handleServerSignal(_ signal: Data?) {
        Task {
            do {
                try await handleServerSignal(signal)
            } catch {
                IKLogger.xpc.error("[KD] signal error :\(error)")

                SentrySDK.capture(message: "Error processing Signal") { scope in
                    scope.setLevel(.error)
                    scope.setContext(
                        value: ["error": error, "description": error.localizedDescription],
                        key: "underlying error"
                    )
                }
            }
        }
    }

    private func handleServerSignal(_ signal: Data?) async throws {
        guard let signal else {
            throw SignalError.nilData
        }

        guard let signalMetadata = try? decoder.decode(SignalMetadata.self, from: signal) else {
            let output = String(data: signal, encoding: .utf8) ?? ""
            throw SignalError.unableToParseMetadata(output)
        }

        guard signalMetadata.code == nil, signalMetadata.cause == nil else {
            throw SignalError.serverError(signalMetadata.code, signalMetadata.cause)
        }

        let signalNum = signalMetadata.num
        IKLogger.xpc.log("[KD] recv signal: \(signalNum)")
        //IKLogger.xpc.log("[KD] recv signal raw: \(String(data: signal, encoding: .utf8))")

        switch signalNum {
        case .USER_ADDED, .USER_UPDATED:
            try await handleUser(signal)

        case .USER_REMOVED:
            try await handleUserRemoved(signal)

        case .ACCOUNT_ADDED:
            try await handleAccountAdded(signal)

        case .ACCOUNT_UPDATED:
            try await handleAccountUpdated(signal)

        case .ACCOUNT_REMOVED:
            try await handleAccountRemoved(signal)

        case .DRIVE_ADDED:
            try await handleDrive(signal)

        case .DRIVE_UPDATED:
            try await handleDrive(signal)

        case .DRIVE_REMOVED:
            try await handleDriveRemoved(signal)

        case .SYNC_ADDED:
            try await handleSync(signal)

        case .SYNC_UPDATED:
            try await handleSync(signal)

        case .SYNC_REMOVED:
            try await handleSyncRemoved(signal)

        case .SYNC_PROGRESSINFO:
            try await handleSyncProgress(signal)

        case .SYNC_COMPLETEDITEM:
            try await handleSyncCompleted(signal)

        default:
            throw SignalError.unsupported(signalNum)
        }
    }

    // MARK: Account

    private func handleAccountAdded(_ signal: Data) async throws {
        guard let accountInfoSignal = try? decoder.decode(SignalMessage<AccountInfoSignal>.self, from: signal) else {
            throw SignalError.unableToGetAccountFromSignal
        }

        let accountInfo = accountInfoSignal.body.accountInfo
        await coherentCache.addAccount(accountInfo.asAccount, userDbId: accountInfo.userDbId)
    }

    private func handleAccountUpdated(_ signal: Data) async throws {
        guard let accountInfoSignal = try? decoder.decode(SignalMessage<AccountInfoSignal>.self, from: signal) else {
            throw SignalError.unableToGetAccountFromSignal
        }

        let accountInfo = accountInfoSignal.body.accountInfo
        try await coherentCache.updateAccount(accountInfo.asAccount)
    }

    private func handleAccountRemoved(_ signal: Data) async throws {
        guard let accountInfoSignal = try? decoder.decode(SignalMessage<AccountRemoveSignal>.self, from: signal) else {
            throw SignalError.unableToGetAccountDbIdFromSignal
        }

        let accountDbId = accountInfoSignal.body.accountDbId
        await coherentCache.removeAccount(accountDbId: accountDbId)
    }

    // MARK: Drive

    private func handleDrive(_ signal: Data) async throws {
        guard let driveInfoSignal = try? decoder.decode(SignalMessage<DriveInfoSignal>.self, from: signal) else {
            throw SignalError.unableToGetDriveFromSignal
        }

        let driveInfo = driveInfoSignal.body.driveInfo
        try await coherentCache.addOrUpdateDriveSignal(driveInfo)
    }

    private func handleDriveRemoved(_ signal: Data) async throws {
        guard let driveInfoSignal = try? decoder.decode(SignalMessage<DriveRemoveSignal>.self, from: signal) else {
            throw SignalError.unableToGetDriveDbIdFromSignal
        }

        let driveDbId = driveInfoSignal.body.driveDbId
        try await coherentCache.removeDrive(driveDbId: driveDbId)
    }

    // MARK: Synchro

    private func handleSync(_ signal: Data) async throws {
        guard let syncInfoSignal = try? decoder.decode(SignalMessage<SyncInfoSignal>.self, from: signal) else {
            throw SignalError.unableToGetSyncFromSignal
        }

        let syncInfo = syncInfoSignal.body.syncInfo
        try await coherentCache.addSynchro(syncInfo.asSynchro)
    }

    private func handleSyncRemoved(_ signal: Data) async throws {
        guard let syncRemoveSignal = try? decoder.decode(SignalMessage<SyncRemoveSignal>.self, from: signal) else {
            throw SignalError.unableToGetSyncDbIdFromSignal
        }

        let syncDbId = syncRemoveSignal.body.syncDbId
        try await coherentCache.removeSynchro(synchroDbId: syncDbId)
    }

    private func handleSyncProgress(_ signal: Data) async throws {
        guard let syncProgressSignal = try? decoder.decode(SignalMessage<SyncProgressInfoSignal>.self, from: signal) else {
            throw SignalError.unableToGetSyncProgressFromSignal
        }

        let syncProgress = syncProgressSignal.body
        try await coherentCache.updateSyncProgressInfoSignal(syncProgress)
    }

    private func handleSyncCompleted(_ signal: Data) async throws {
        guard let syncFileItemInfo = try? decoder.decode(SignalMessage<SyncFileItemInfoSignal>.self, from: signal) else {
            throw SignalError.unableToGetSyncFileItemFromSignal
        }

        let syncFileItem = syncFileItemInfo.body
        try await coherentCache.updateSyncFileItemInfoSignal(syncFileItem)
    }
}

extension CoherentCache {
    func updateSyncProgressInfoSignal(_ syncSignal: SyncProgressInfoSignal) async throws {
        let syncDbId = syncSignal.syncDbId
        guard var synchro = await getSynchro(synchroDbId: syncDbId) else {
            throw ServerCoherentCache.CacheError.synchroNotFound(syncDbId)
        }

        synchro.progress = syncSignal.asSynchroProgressInfo

        try await updateSynchro(synchro)
    }
}

extension CoherentCache {
    func updateSyncFileItemInfoSignal(_ syncSignal: SyncFileItemInfoSignal) async throws {
        let syncDbId = syncSignal.syncDbId
        let itemInfo = syncSignal.itemInfo
        guard var synchro = await getSynchro(synchroDbId: syncDbId) else {
            throw ServerCoherentCache.CacheError.synchroNotFound(syncDbId)
        }

        synchro.addOrUpdateSynchNode(itemInfo.toSynchroFile)

        try await updateSynchro(synchro)
    }
}

extension CoherentCache {
    func addOrUpdateDriveSignal(_ driveSignal: DriveInfoSignalMetadata) async throws {
        let accountDbId = driveSignal.accountDbId
        guard var account = await getAccount(accountDbId: accountDbId) else {
            throw ServerCoherentCache.CacheError.accountNotFound(accountDbId)
        }

        let existingDrive = account.drives[driveSignal.dbId]
        let updatedDrive = driveSignal.asDrive(accountId: account.id,
                                               userDbId: account.userDbId,
                                               synchros: existingDrive?.synchros ?? [:])
        account.drives[driveSignal.dbId] = updatedDrive

        try await updateAccount(account)
    }
}
