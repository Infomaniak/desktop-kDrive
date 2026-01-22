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

public enum NewSyncParentIdentifier: Sendable {
    case driveDbId(Int32)
    case transitive(userDbId: Int32, accountId: Int32, driveId: Int32)
}

public struct NewSyncMetadata: Sendable {
    let userDbId: Int32
    let accountId: Int32
    let driveId: Int32
    let localFolderPath: String
    let serverFolderPath: String
    let serverFolderNodeId: String
    let liteSync: Bool
    let blackList: [String]
    let whiteList: [String]

    public init(
        userDbId: Int32,
        accountId: Int32,
        driveId: Int32,
        localFolderPath: String,
        serverFolderPath: String,
        serverFolderNodeId: String,
        liteSync: Bool,
        blackList: [String],
        whiteList: [String]
    ) {
        self.userDbId = userDbId
        self.accountId = accountId
        self.driveId = driveId
        self.localFolderPath = localFolderPath
        self.serverFolderPath = serverFolderPath
        self.serverFolderNodeId = serverFolderNodeId
        self.liteSync = liteSync
        self.blackList = blackList
        self.whiteList = whiteList
    }
}

public struct SyncJobs: Sendable {
    @LazyInjectService private var coherentCache: CoherentCache
    @LazyInjectService private var queryFetcher: XPCQueryFetcherProtocol

    public init() {}

    @discardableResult
    public func availableSync() async throws -> [SyncInfo] {
        IKLogger.data.log("Query for availableSync list")
        let query = EmptyQuery()
        let request = await RequestMessage<EmptyQuery>(num: RequestNum.SYNC_INFOLIST, body: query)

        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<SyncInfoList>.self)

        try decodedMessage.validate()

        let syncList = decodedMessage.body.syncInfoList
        await syncList.asyncForEach { try? await coherentCache.updateSynchro($0.asSynchro) }

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

        let syncStatus = decodedMessage.body.syncStatus

        // TODO: update existing sync state in cache
        // await coherentCache.updateSynchroState(syncDbId / newSyncState)

        return syncStatus
    }

    public func addSync(identifier: NewSyncParentIdentifier, metadata: NewSyncMetadata) async throws -> SyncInfo {
        IKLogger.data.log("Query to addSync")

        switch identifier {
        case .transitive(let userDbId, let accountId, let driveId):
            let newSyncQuery = NewSyncQuery(userDbId: userDbId,
                                            accountId: accountId,
                                            driveId: driveId,
                                            metadata: metadata)
            return try await addSync(newSyncQuery)
        case .driveDbId(let driveDbId):
            let newSyncQuery = NewSyncQueryAlternate(driveDbId: driveDbId, metadata: metadata)
            return try await addSync(newSyncQuery)
        }
    }

    private func addSync(_ newSyncQuery: NewSyncQuery) async throws -> SyncInfo {
        let request = await RequestMessage<NewSyncQuery>(num: RequestNum.SYNC_ADD, body: newSyncQuery)
        return try await addSyncQuery(request, responseType: CallbackMessage<SyncInfoSingle>.self)
    }

    private func addSync(_ newSyncQuery: NewSyncQueryAlternate) async throws -> SyncInfo {
        let request = await RequestMessage<NewSyncQueryAlternate>(num: RequestNum.SYNC_ADD2, body: newSyncQuery)
        return try await addSyncQuery(request, responseType: CallbackMessage<SyncInfoSingle>.self)
    }

    private func addSyncQuery<Response: Decodable>(_ request: Encodable, responseType: Response.Type) async throws -> SyncInfo {
        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<SyncInfoSingle>.self)
        try decodedMessage.validate()

        // TODO: bump cache / listen signal
        return decodedMessage.body.syncInfo
    }

    public func syncStartAfterLoginJob(userDbId: Int32) async throws {
        IKLogger.data.log("Query for userDelete")
        let query = UserQuery(userDbId: userDbId)
        let request = await RequestMessage<UserQuery>(num: RequestNum.SYNC_START_AFTER_LOGIN, body: query)

        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<EmptyResponse>.self)

        try decodedMessage.validate()
    }

    public func syncDelete(syncDbId: Int32) async throws {
        IKLogger.data.log("Query to syncDelete")
        let query = SyncQuery(syncDbId: syncDbId)
        let request = await RequestMessage<SyncQuery>(num: RequestNum.SYNC_DELETE, body: query)

        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<EmptyResponse>.self)

        try decodedMessage.validate()

        // TODO: clear cache based only on syncDbId
        // coherentCache.removeSynchro(syncDbId, fromDrive: <#T##Int32#>, accountId: <#T##Int32#>, userDbId: <#T##Int32#>)
    }

    public func getPublicLinkUrl(driveDbId: Int32, nodeId: String) async throws {
        IKLogger.data.log("Query to syncGetPublicLinkUrl")
        let query = PublicLinkQuery(driveDbId: driveDbId, nodeId: nodeId)
        let request = await RequestMessage<PublicLinkQuery>(num: RequestNum.SYNC_GETPUBLICLINKURL, body: query)

        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<PublicLinkResponse>.self)

        try decodedMessage.validate()
    }
}
