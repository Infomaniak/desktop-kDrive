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
import InfomaniakConcurrency
import InfomaniakDI

public struct SyncJobs: Sendable {
    @LazyInjectService private var coherentCache: CoherentCacheProtocol
    @LazyInjectService private var queryFetcher: XPCQueryFetcherProtocol

    public init() {}

    public func availableSync() async throws -> [SyncInfo] {
        IKLogger.data.log("Query for availableSync list")
        let query = EmptyQuery()
        let request = await RequestMessage<EmptyQuery>(num: RequestNum.SYNC_INFOLIST, body: query)

        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<SyncInfoList>.self)

        try decodedMessage.validate()

        let syncList = decodedMessage.body.syncInfoList

        // TODO: Check if this needs cache (prolly not)
        await syncList.asyncForEach { await coherentCache.updateSynchro($0.asSynchro) }

        return syncList
    }

    public func startSync(syncDbId: Int32) async throws {
        IKLogger.data.log("Query to startSync")
        let query = SyncQuery(syncDbId: syncDbId)
        let request = await RequestMessage<SyncQuery>(num: RequestNum.SYNC_START, body: query)

        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<EmptyResponse>.self)

        try decodedMessage.validate()
    }

    public func stopSync(syncDbId: Int32) async throws {
        IKLogger.data.log("Query to stopSync")
        let query = SyncQuery(syncDbId: syncDbId)
        let request = await RequestMessage<SyncQuery>(num: RequestNum.SYNC_STOP, body: query)

        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<EmptyResponse>.self)

        try decodedMessage.validate()
    }

    public func syncStatus(syncDbId: Int32) async throws -> KDC.SyncFileStatus {
        IKLogger.data.log("Query for syncStatus")
        let query = SyncQuery(syncDbId: syncDbId)
        let request = await RequestMessage<SyncQuery>(num: RequestNum.SYNC_STATUS, body: query)

        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<SyncStatusResponse>.self)

        try decodedMessage.validate()

        return decodedMessage.body.syncStatus
    }

    // TODO, clean init
    public func addSync(_ newSyncQuery: NewSyncQuery) async throws -> SyncInfo {
        IKLogger.data.log("Query to addSync")
        let request = await RequestMessage<NewSyncQuery>(num: RequestNum.SYNC_ADD, body: newSyncQuery)
        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<SyncInfoSingle>.self)
        try decodedMessage.validate()

        return decodedMessage.body.syncInfo
    }

    // TODO, clean init
    public func addSync(_ newSyncQuery: NewSyncQueryAlternate) async throws -> SyncInfo {
        IKLogger.data.log("Query to addSync")
        let request = await RequestMessage<NewSyncQueryAlternate>(num: RequestNum.SYNC_ADD2, body: newSyncQuery)
        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<SyncInfoSingle>.self)
        try decodedMessage.validate()

        return decodedMessage.body.syncInfo
    }
}
