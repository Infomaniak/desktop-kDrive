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

import CppInterop
import Foundation
import InfomaniakDI

public struct UtilityJobs: Sendable {
    @LazyInjectService private var queryFetcher: XPCQueryFetcherProtocol

    enum UtilityError: Error {
        case noGoodNewSynchPath(message: String)
    }

    public init() {}

    public func getBestVirtualFileSystemMode(path: String) async throws -> KDC.VirtualFileMode {
        let query = UtilityBestVFSQuery(path: path)
        let request = await RequestMessage<UtilityBestVFSQuery>(num: RequestNum.UTILITY_BESTVFSAVAILABLEMODE, body: query)

        let decodedMessage = try await queryFetcher.query(request, responseType: CallbackMessage<UtilityBestVFSResponse>.self)

        return decodedMessage.body.bestMode
    }

    public func getGoodPathForNewSynchro(basePath: String) async throws -> String {
        let query = UtilityGoodPathNewSyncQuery(basePath: basePath)
        let request = await RequestMessage<UtilityGoodPathNewSyncQuery>(
            num: RequestNum.UTILITY_FINDGOODPATHFORNEWSYNC,
            body: query
        )

        let decodedMessage = try await queryFetcher.query(
            request,
            responseType: CallbackMessage<UtilityGoodPathNewSyncResponse>.self
        )

        guard decodedMessage.body.errorMessage.isEmpty else {
            throw UtilityError.noGoodNewSynchPath(message: decodedMessage.body.errorMessage)
        }

        return decodedMessage.body.goodPath
    }

    public func isPathValidFor(path: String, syncConfiguration: KDC.SyncConfiguration) async throws -> Bool {
        let query = UtilityIsPathValidForNewSyncQuery(path: path, syncConfiguration: syncConfiguration)
        let request = await RequestMessage<UtilityIsPathValidForNewSyncQuery>(
            num: RequestNum.UTILITY_ISPATHVALIDFORNEWSYNC,
            body: query
        )

        let decodedMessage = try await queryFetcher.query(
            request,
            responseType: CallbackMessage<UtilityIsPathValidForNewSyncResponse>.self
        )

        return decodedMessage.body.isValid
    }

    public func activateLoadInfo() async throws {
        let request = await RequestMessage<EmptyQuery>(num: RequestNum.UTILITY_ACTIVATELOADINFO, body: EmptyQuery())

        try await queryFetcher.query(request, responseType: CallbackMessage<EmptyResponse>.self)
    }

    public func checkCommStatus() async throws {
        let request = await RequestMessage<EmptyQuery>(num: RequestNum.UTILITY_CHECKCOMMSTATUS, body: EmptyQuery())

        try await queryFetcher.query(request, responseType: CallbackMessage<EmptyResponse>.self)
    }

    public func hasSystemLaunchOnStartup() async throws -> Bool {
        let request = await RequestMessage<EmptyQuery>(
            num: RequestNum.UTILITY_HASSYSTEMLAUNCHONSTARTUP,
            body: EmptyQuery()
        )

        let decodedMessage = try await queryFetcher.query(
            request,
            responseType: CallbackMessage<UtilityHasSystemLaunchOnStartupResponse>.self
        )

        return decodedMessage.body.enabled
    }

    public func hasLaunchOnStartup() async throws -> Bool {
        let request = await RequestMessage<EmptyQuery>(
            num: RequestNum.UTILITY_HASLAUNCHONSTARTUP,
            body: EmptyQuery()
        )

        let decodedMessage = try await queryFetcher.query(
            request,
            responseType: CallbackMessage<UtilityHasLaunchOnStartupResponse>.self
        )

        return decodedMessage.body.enabled
    }

    public func setLaunchOnStartup(enabled: Bool) async throws {
        let query = UtilitySetLaunchOnStartupQuery(enabled: enabled)
        let request = await RequestMessage<UtilitySetLaunchOnStartupQuery>(
            num: RequestNum.UTILITY_SETLAUNCHONSTARTUP,
            body: query
        )

        try await queryFetcher.query(
            request,
            responseType: CallbackMessage<EmptyResponse>.self
        )
    }

    public func setAppState(key: Int32, value: Int32) async throws {
        let query = UtilitySetAppStateQuery(key: key, value: value)
        let request = await RequestMessage<UtilitySetAppStateQuery>(
            num: RequestNum.UTILITY_SET_APPSTATE,
            body: query
        )

        try await queryFetcher.query(
            request,
            responseType: CallbackMessage<EmptyResponse>.self
        )
    }

    public func getAppState(key: Int32) async throws -> Int32 {
        let query = UtilityGetAppStateQuery(key: key)
        let request = await RequestMessage<UtilityGetAppStateQuery>(
            num: RequestNum.UTILITY_GET_APPSTATE,
            body: query
        )

        let decodedMessage = try await queryFetcher.query(
            request,
            responseType: CallbackMessage<UtilityGetAppStateResponse>.self
        )

        return decodedMessage.body.value
    }

    public func sendLogToSupport(includeArchivedLogs: Bool) async throws {
        let query = UtilitySendLogToSupportQuery(includeArchivedLogs: includeArchivedLogs)
        let request = await RequestMessage<UtilitySendLogToSupportQuery>(
            num: RequestNum.UTILITY_SEND_LOG_TO_SUPPORT,
            body: query
        )

        try await queryFetcher.query(
            request,
            responseType: CallbackMessage<EmptyResponse>.self
        )
    }

    public func cancelLogToSupport() async throws {
        let request = await RequestMessage<EmptyQuery>(
            num: RequestNum.UTILITY_CANCEL_LOG_TO_SUPPORT,
            body: EmptyQuery()
        )

        try await queryFetcher.query(
            request,
            responseType: CallbackMessage<EmptyResponse>.self
        )
    }

    public func crash() async throws {
        let request = await RequestMessage<EmptyQuery>(num: RequestNum.UTILITY_CRASH, body: EmptyQuery())

        try await queryFetcher.query(request, responseType: CallbackMessage<EmptyResponse>.self)
    }

    public func quit() async throws {
        let request = await RequestMessage<EmptyQuery>(num: RequestNum.UTILITY_QUIT, body: EmptyQuery())

        try await queryFetcher.query(request, responseType: CallbackMessage<EmptyResponse>.self)
    }

    public func sendAppStartTrace() async throws {
        let request = await RequestMessage<EmptyQuery>(
            num: RequestNum.UTILITY_SEND_APP_START_TRACE,
            body: EmptyQuery()
        )

        try await queryFetcher.query(
            request,
            responseType: CallbackMessage<EmptyResponse>.self
        )
    }

    public func installLiteSyncExtension() async throws {
        let request = await RequestMessage<EmptyQuery>(
            num: RequestNum.UTILITY_INSTALL_MAC_LITESYNC_EXT,
            body: EmptyQuery()
        )

        try await queryFetcher.query(
            request,
            responseType: CallbackMessage<EmptyResponse>.self
        )
    }

    public func checkMacOsPermissions() async throws -> UtilityCheckMacOsPermissionsResponse {
        let request = await RequestMessage<EmptyQuery>(
            num: RequestNum.UTILITY_CHECK_MACOS_PERMISSIONS,
            body: EmptyQuery()
        )

        let decodedMessage = try await queryFetcher.query(
            request,
            responseType: CallbackMessage<UtilityCheckMacOsPermissionsResponse>.self
        )

        return decodedMessage.body
    }
}
