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
    func handleServerSignal(_ signal: Data?) async
}

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
    case unableToGetNotificationFromSignal
    case unableToGetErrorInfoFromSignal
    case unableToGetErrorRemovedFromSignal
    case unableToGetVersionInfoFromSignal
    case unableToGetUpdateStateFromSignal
    case unsupported(_ num: SignalNum)
}

struct XPCSignalHandler: XPCSignalHandlerProtocol {
    private let decoder = JSONDecoder()
    private let userHandler = UserSignalHandler()
    private let accountHandler = AccountSignalHandler()
    private let driveHandler = DriveSignalHandler()
    private let synchroHandler = SynchroSignalHandler()
    private let utilitySignalHandler = UtilitySignalHandler()
    private let updaterSignalHandler = UpdaterSignalHandler()

    func handleServerSignal(_ signal: Data?) async {
        do {
            try await decodeAndUseServerSignal(signal)
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

    private func decodeAndUseServerSignal(_ signal: Data?) async throws {
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
        // IKLogger.xpc.log("[KD] recv signal raw: \(String(data: signal, encoding: .utf8))")

        switch signalNum {
        case .USER_ADDED, .USER_UPDATED:
            try await userHandler.handleUser(signal)

        case .USER_REMOVED:
            try await userHandler.handleUserRemoved(signal)

        case .ACCOUNT_ADDED, .ACCOUNT_UPDATED:
            try await accountHandler.handleAccountAddedOrUpdated(signal)

        case .ACCOUNT_REMOVED:
            try await accountHandler.handleAccountRemoved(signal)

        case .DRIVE_ADDED, .DRIVE_UPDATED:
            try await driveHandler.handleDriveAddedOrUpdated(signal)

        case .DRIVE_REMOVED:
            try await driveHandler.handleDriveRemoved(signal)

        case .SYNC_ADDED, .SYNC_UPDATED:
            try await synchroHandler.handleSyncAddedOrUpdated(signal)

        case .SYNC_REMOVED:
            try await synchroHandler.handleSyncRemoved(signal)

        case .SYNC_PROGRESSINFO:
            try await synchroHandler.handleSyncProgress(signal)

        case .SYNC_COMPLETEDITEM:
            try await synchroHandler.handleSyncCompleted(signal)

        case .UTILITY_SHOW_NOTIFICATION:
            try await utilitySignalHandler.handleShowNotification(signal)

        case .UTILITY_ERROR_ADDED:
            try await utilitySignalHandler.handleError(signal)

        case .UTILITY_ERROR_REMOVED:
            try await utilitySignalHandler.handleErrorRemoved(signal)

        case .UTILITY_ERRORS_CLEARED: // Soon legacy Signal
            try await utilitySignalHandler.handleErrorCleared()

        case .UPDATER_SHOW_DIALOG:
            try await updaterSignalHandler.handleShowDialog(signal)

        case .UPDATER_STATE_CHANGED:
            try await updaterSignalHandler.handleStateChanged(signal)

        default:
            throw SignalError.unsupported(signalNum)
        }
    }
}
