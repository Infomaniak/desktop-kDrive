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

public struct DriveJobs: Sendable {
    @LazyInjectService private var coherentCache: CoherentCacheProtocol
    @LazyInjectService private var queryFetcher: XPCQueryFetcherProtocol

    public enum DriveJobsError: Error {
        case responseListNotFound
        case noReplyData
    }

    public init() {}

    public func availableDrives(userDbId: Int32) async throws -> [DriveResponse] {
        IKLogger.data.log("Query for availableDrives list")
        let query = DriveListQuery(userDbId: userDbId)
        let request = await RequestMessage<DriveListQuery>(num: RequestNum.USER_AVAILABLEDRIVES, body: query)

        do {
            let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<DriveListResponse>.self)

            guard let driveList = decodedMessage?.body.driveAvailableInfoList else {
                throw DriveJobsError.responseListNotFound
            }

            await driveList.asyncForEach { await coherentCache.updateDrive(drive: $0.asDrive) }

            return driveList
        } catch XPCQueryFetcher.QueryError.noReplyData {
            throw DriveJobsError.noReplyData
        }
    }
}
