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

    func handleServerSignal(_ signal: Data?) {
        Task {
            await handleServerSignal(signal)
        }
    }

    private func handleServerSignal(_ signal: Data?) async {
        guard let signal else {
            IKLogger.xpc.error("[KD] recv sendSignal with nil data")
            return
        }

        guard let signalMetadata = try? decoder.decode(SignalMetadata.self, from: signal) else {
            let output = String(data: signal, encoding: .utf8)
            IKLogger.xpc.error("[KD] recv unable to parse signal \(String(describing: output))")
            return
        }

        guard signalMetadata.code == nil, signalMetadata.cause == nil else {
            IKLogger.xpc.error("[KD] recv error code:\(String(describing: signalMetadata.code)) cause:\(String(describing: signalMetadata.cause))")
            return
        }

        let signalNum = signalMetadata.num
        IKLogger.xpc.log("[KD] recv signal: \(signalNum)")

        switch signalNum {
        case .USER_ADDED:
            await handleUserAdded(signal)

        case .USER_UPDATED:
            IKLogger.xpc.error("[KD] TODO - USER_UPDATED not available")

        case .ACCOUNT_ADDED:
            await handleAccountAdded(signal)

        case .ACCOUNT_UPDATED:
            IKLogger.xpc.error("[KD] TODO - ACCOUNT_UPDATED not available")

        case .ACCOUNT_REMOVED:
            await handleAccountRemoved(signal)

        case .DRIVE_UPDATED:
            await handleDriveUpdated(signal)

        case .DRIVE_ADDED:
            IKLogger.xpc.log("[KD] TODO - drive signal")

        default:
            IKLogger.xpc.error("[KD] recv error code:\(String(describing: signalMetadata.code)) cause:\(String(describing: signalMetadata.cause))")
        }
    }

    // MARK: - Specific signal handling

    private func handleUserAdded(_ signal: Data) async {
        guard let userInfoSignal = try? decoder.decode(SignalMessage<UserInfoSignal>.self, from: signal),
              let user = userInfoSignal.body?.asUser else {
            IKLogger.xpc.error("[KD] Unable to get user from signal")
            return
        }

        await coherentCache.updateUser(user)
    }

    private func handleAccountAdded(_ signal: Data) async {
        guard let accountInfoSignal = try? decoder.decode(SignalMessage<AccountInfoSignal>.self, from: signal),
              let accountInfo = accountInfoSignal.body else {
            IKLogger.xpc.error("[KD] Unable to get account from signal")
            return
        }

        await coherentCache.addAccount(accountInfo.asAccount, userDbId: accountInfo.userDbId)
    }

    private func handleAccountRemoved(_ signal: Data) async {
        guard let accountInfoSignal = try? decoder.decode(SignalMessage<AccountRemoveSignal>.self, from: signal),
              let accountDbId = accountInfoSignal.body?.accountDbId else {
            IKLogger.xpc.error("[KD] Unable to get accountDbId from signal")
            return
        }

        await coherentCache.removeAccount(accountDbId: accountDbId)
    }

    private func handleDriveUpdated(_ signal: Data) async {
        guard let driveInfoSignal = try? decoder.decode(SignalMessage<DriveInfoSignal>.self, from: signal),
              let driveInfo = driveInfoSignal.body else {
            IKLogger.xpc.error("[KD] Unable to get driveInfo from signal")
            return
        }

        try? await coherentCache.updateDriveSignal(driveInfo)
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
