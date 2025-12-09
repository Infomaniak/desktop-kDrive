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
        case unableToGetAccountFromSignal
        case unableToGetAccountDbIdFromSignal
        case unableToGetDriveFromSignal
        case unableToGetDriveDbIdFromSignal
        case unsupported(_ num: SignalNum)
    }

    func handleServerSignal(_ signal: Data?) {
        Task {
            do {
                try await handleServerSignal(signal)
            } catch {
                IKLogger.xpc.error("[KD] signal error :\(error)")
                // TODO: Sentry
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

        switch signalNum {
        case .USER_ADDED:
            try await handleUserAdded(signal)

        case .USER_UPDATED:
            IKLogger.xpc.error("[KD] TODO - USER_UPDATED not available")
            throw SignalError.unsupported(signalNum)

        case .ACCOUNT_ADDED:
            try await handleAccountAdded(signal)

        case .ACCOUNT_UPDATED:
            IKLogger.xpc.error("[KD] TODO - ACCOUNT_UPDATED not available")
            throw SignalError.unsupported(signalNum)

        case .ACCOUNT_REMOVED:
            try await handleAccountRemoved(signal)

        case .DRIVE_UPDATED:
            try await handleDriveUpdated(signal)

        case .DRIVE_ADDED:
            IKLogger.xpc.log("[KD] TODO - DRIVE_ADDED not available")
            throw SignalError.unsupported(signalNum)

        default:
            throw SignalError.unsupported(signalNum)
        }
    }

    // MARK: - Specific signal handling

    private func handleUserAdded(_ signal: Data) async throws {
        guard let userInfoSignal = try? decoder.decode(SignalMessage<UserInfoSignal>.self, from: signal),
              let user = userInfoSignal.body?.asUser else {
            throw SignalError.unableToGetUserFromSignal
        }

        await coherentCache.updateUser(user)
    }

    private func handleAccountAdded(_ signal: Data) async throws {
        guard let accountInfoSignal = try? decoder.decode(SignalMessage<AccountInfoSignal>.self, from: signal),
              let accountInfo = accountInfoSignal.body else {
            throw SignalError.unableToGetAccountFromSignal
        }

        await coherentCache.addAccount(accountInfo.asAccount, userDbId: accountInfo.userDbId)
    }

    private func handleAccountRemoved(_ signal: Data) async throws {
        guard let accountInfoSignal = try? decoder.decode(SignalMessage<AccountRemoveSignal>.self, from: signal),
              let accountDbId = accountInfoSignal.body?.accountDbId else {
            throw SignalError.unableToGetAccountDbIdFromSignal
        }

        await coherentCache.removeAccount(accountDbId: accountDbId)
    }

    private func handleDriveUpdated(_ signal: Data) async throws {
        guard let driveInfoSignal = try? decoder.decode(SignalMessage<DriveInfoSignal>.self, from: signal),
              let driveInfo = driveInfoSignal.body else {
            throw SignalError.unableToGetDriveFromSignal
        }

        try await coherentCache.updateDriveSignal(driveInfo)
    }
}

extension CoherentCache {
    func updateDriveSignal(_ driveSignal: DriveInfoSignal) async throws {
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
