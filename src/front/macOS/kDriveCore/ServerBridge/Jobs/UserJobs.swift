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

public struct UserJobs: Sendable {
    @LazyInjectService private var coherentCache: CoherentCacheProtocol
    @LazyInjectService private var queryFetcher: XPCQueryFetcherProtocol

    public enum UserJobError: Error {
        case responseListNotFound
        case noReplyData
    }

    public init() {}

    public func userDbIds() async throws -> [Int32] {
        IKLogger.data.log("Query for userDbIds list")
        let request = await RequestMessage<EmptyQuery>(num: RequestNum.USER_DBIDLIST, body: EmptyQuery())

        do {
            let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<UserDbIdsListResponse>.self)

            guard let userDbIds = decodedMessage?.body.userDbIdList else {
                throw UserJobError.responseListNotFound
            }

            return userDbIds
        } catch XPCQueryFetcher.QueryError.noReplyData {
            throw UserJobError.noReplyData
        }
    }

    public func userInfoList() async throws -> [UserInfoResponse] {
        IKLogger.data.log("Query for userInfo list")
        let request = await RequestMessage<EmptyQuery>(num: RequestNum.USER_INFOLIST, body: EmptyQuery())

        do {
            let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<UserInfoListResponse>.self)

            guard let userList = decodedMessage?.body.userInfoList else {
                throw UserJobError.responseListNotFound
            }

            await userList.asyncForEach { await coherentCache.updateUser($0.userCache) }

            return userList
        } catch XPCQueryFetcher.QueryError.noReplyData {
            throw UserJobError.noReplyData
        }
    }

    public func userDelete(dbId: Int32) async throws {
        IKLogger.data.log("Query for userDelete")
        let query = UserDeleteQuery(userDbId: dbId)
        let request = await RequestMessage<UserDeleteQuery>(num: RequestNum.USER_DELETE, body: query)

        do {
            guard try await queryFetcher.query(request, responseType: CallbackMessage<EmptyResponse>.self) != nil else {
                throw UserJobError.noReplyData
            }

            await coherentCache.removeUser(dbId: dbId)
        } catch XPCQueryFetcher.QueryError.noReplyData {
            throw UserJobError.noReplyData
        }
    }
}
