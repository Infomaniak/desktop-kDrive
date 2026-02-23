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

struct SynchroSignalHandler {
    private let decoder = JSONDecoder()
    @LazyInjectService private var coherentCache: CoherentCache

    func handleSyncAddedOrUpdated(_ signal: Data) async throws {
        guard let syncInfoSignal = try? decoder.decode(SignalMessage<SyncInfoSignal>.self, from: signal) else {
            throw SignalError.unableToGetSyncFromSignal
        }

        let syncInfo = syncInfoSignal.body.syncInfo
        try await coherentCache.addSynchro(syncInfo.asSynchro)
    }

    func handleSyncRemoved(_ signal: Data) async throws {
        guard let syncRemoveSignal = try? decoder.decode(SignalMessage<SyncRemoveSignal>.self, from: signal) else {
            throw SignalError.unableToGetSyncDbIdFromSignal
        }

        let syncDbId = syncRemoveSignal.body.syncDbId
        try await coherentCache.removeSynchro(synchroDbId: syncDbId)
    }

    func handleSyncProgress(_ signal: Data) async throws {
        guard let syncProgressSignal = try? decoder.decode(SignalMessage<SyncProgressInfoSignal>.self, from: signal) else {
            throw SignalError.unableToGetSyncProgressFromSignal
        }

        let syncProgress = syncProgressSignal.body
        try await coherentCache.updateSyncProgressInfoSignal(syncProgress)
    }

    func handleSyncCompleted(_ signal: Data) async throws {
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
