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
import OrderedCollections

public extension CoherentCache {
    func addDriveResponse(_ driveResponse: DriveResponse) async throws {
        let accountDbId = driveResponse.accountDbId
        guard var account = await getAccount(accountDbId: accountDbId) else {
            throw ServerCoherentCache.CacheError.accountNotFound(accountDbId)
        }

        let drive = driveResponse.asDrive(accountId: account.id,
                                          userDbId: account.userDbId)
        account.drives[drive.driveDbId] = drive

        try await updateAccount(account)
    }
}

public struct DriveJobs: Sendable {
    @LazyInjectService private var coherentCache: CoherentCache
    @LazyInjectService private var queryFetcher: XPCQueryFetcherProtocol

    public init() {}

    @discardableResult
    public func driveInfoList() async throws -> [DriveResponse] {
        IKLogger.data.log("Query for driveInfoList")
        let query = EmptyQuery()
        let request = await RequestMessage<EmptyQuery>(num: RequestNum.DRIVE_INFOLIST, body: query)

        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<DriveInfoListResponse>.self)

        let driveList = decodedMessage.body.driveInfoList

        await driveList.asyncForEach { try? await coherentCache.addDriveResponse($0) }

        return driveList
    }

    public func availableDrives(userDbId: Int32) async throws -> [AvailableDriveResponse] {
        IKLogger.data.log("Query for availableDrives list")
        let query = DriveListQuery(userDbId: userDbId)
        let request = await RequestMessage<DriveListQuery>(num: RequestNum.USER_AVAILABLEDRIVES, body: query)

        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<DriveListResponse>.self)

        let driveList = decodedMessage.body.driveAvailableInfoList
        let availableDrives = driveList.map { $0.asAvailableDrive }
        try? await coherentCache.updateAvailableDrives(availableDrives, forUserDbId: userDbId)

        return driveList
    }

    public func driveUpdate(driveInfo: AvailableDriveResponse) async throws {
        IKLogger.data.log("Query to update drive: \(driveInfo.driveId)")
        let query = DriveUpdateQuery(driveInfo: driveInfo)
        let request = await RequestMessage<DriveUpdateQuery>(num: RequestNum.DRIVE_UPDATE, body: query)

        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<EmptyResponse>.self)

        // TODO: update cache by drive update signal
        // await coherentCache.updateDrive(drive: driveInfo.asDrive)
    }

    // TODO: Simplify parameters requirements once I checked with team how to get a `driveDbId` all the time
    public func driveDelete(driveDbId: Int32, accountId: Int32, userDbId: Int32) async throws {
        IKLogger.data.log("Query for driveDelete")
        let query = DriveDeleteQuery(driveDbId: driveDbId)
        let request = await RequestMessage<DriveDeleteQuery>(num: RequestNum.DRIVE_DELETE, body: query)

        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<EmptyResponse>.self)

        await coherentCache.removeDrive(driveDbId: driveDbId, accountDbId: accountId, userDbId: userDbId)

        // TODO: also implement signal
    }

    public func driveSearch(driveDbId: Int32, searchString: String) async throws -> [FileResponse] {
        IKLogger.data.log("Query for driveSearch")
        let query = DriveSearchQuery(driveDbId: driveDbId, searchString: searchString)
        let request = await RequestMessage<DriveSearchQuery>(num: RequestNum.DRIVE_SEARCH, body: query)

        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<SearchInfoList>.self)

        return decodedMessage.body.searchInfoList
    }
}
