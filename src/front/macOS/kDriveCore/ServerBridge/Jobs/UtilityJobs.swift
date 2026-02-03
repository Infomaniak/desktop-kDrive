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

public struct UtilityJobs: Sendable {
    @LazyInjectService private var queryFetcher: XPCQueryFetcherProtocol

    enum UtilityError: Error {
        case noGoodNewSynchPath(message: String)
    }

    public init() {}

    public func getBestVirtualFileSystemMode(path: String, driveDbId: Int32) async throws -> KDC.VirtualFileMode {
        IKLogger.data.log("Query for best VFS mode")
        let query = UtilityBestVFSQuery(path: path, driveDbId: driveDbId)
        let request = await RequestMessage<UtilityBestVFSQuery>(num: RequestNum.UTILITY_BESTVFSAVAILABLEMODE, body: query)
        print("woops \(RequestNum.EXCLAPP_GETLIST)")

        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<UtilityBestVFSResponse>.self)

        try decodedMessage.validate()

        return decodedMessage.body.bestMode
    }

    public func getGoodPathForNewSynchro(basePath: String, driveDbId: Int32) async throws -> String {
        IKLogger.data.log("Query for good Path for new Synchro")
        let query = UtilityGoodPathNewSyncQuery(basePath: basePath, driveDbId: driveDbId)
        let request = await RequestMessage<UtilityGoodPathNewSyncQuery>(
            num: RequestNum.UTILITY_FINDGOODPATHFORNEWSYNC,
            body: query
        )

        let decodedMessage = try await queryFetcher.query(
            request,
            responseType: CallbackMessage<UtilityGoodPathNewSyncResponse>.self
        )

        try decodedMessage.validate()

        guard !decodedMessage.body.errorMessage.isEmpty else {
            throw UtilityError.noGoodNewSynchPath(message: decodedMessage.body.errorMessage)
        }

        return decodedMessage.body.goodPath
    }

    public func isPathValidFor(path: String) async throws -> Bool {
        IKLogger.data.log("Query for path validation for new sync")
        let query = UtilityIsPathValidForNewSyncQuery(path: path)
        let request = await RequestMessage<UtilityIsPathValidForNewSyncQuery>(
            num: RequestNum.UTILITY_ISPATHVALIDFORNEWSYNC,
            body: query
        )

        let decodedMessage = try await queryFetcher.query(
            request,
            responseType: CallbackMessage<UtilityIsPathValidForNewSyncResponse>.self
        )

        try decodedMessage.validate()

        return decodedMessage.body.isValid
    }

    public func activateLoadInfo() async throws {
        IKLogger.data.log("Query for activating load info display")
        let request = await RequestMessage<EmptyQuery>(num: RequestNum.UTILITY_ACTIVATELOADINFO, body: EmptyQuery())

        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<EmptyResponse>.self)

        try decodedMessage.validate()
    }

    public func checkCommStatus() async throws {
        IKLogger.data.log("Query for checking communication status")
        let request = await RequestMessage<EmptyQuery>(num: RequestNum.UTILITY_CHECKCOMMSTATUS, body: EmptyQuery())

        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<EmptyResponse>.self)

        try decodedMessage.validate()
    }

    public func hasSystemLaunchOnStartup() async throws -> Bool {
        IKLogger.data.log("Query for system launch on startup status")
        let request = await RequestMessage<EmptyQuery>(
            num: RequestNum.UTILITY_HASSYSTEMLAUNCHONSTARTUP,
            body: EmptyQuery()
        )

        let decodedMessage = try await queryFetcher.query(
            request,
            responseType: CallbackMessage<UtilityHasSystemLaunchOnStartupResponse>.self
        )

        try decodedMessage.validate()

        return decodedMessage.body.enabled
    }
}
