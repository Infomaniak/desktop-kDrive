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
    func handleServerSignal(_ msg: Data?)
}

struct XPCSignalHandler: XPCSignalHandlerProtocol {
    let decoder = JSONDecoder()

    public func handleServerSignal(_ msg: Data?) {
        guard let msg else {
            IKLogger.xpc.error("[KD] recv sendSignal with nil data")
            return
        }

        guard let signalMetadata = try? decoder.decode(SignalMetadata.self, from: msg) else {
            let output = String(data: msg, encoding: .utf8)
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
        case .USER_UPDATED, .USER_ADDED:
            guard let userInfoSignal = try? decoder.decode(SignalMessage<UserInfoSignal>.self, from: msg),
                  let user = userInfoSignal.body?.asUser else {
                IKLogger.xpc.error("[KD] Unable to get user from signal")
                return
            }

            Task {
                @InjectService var coherentCache: CoherentCache
                await coherentCache.updateUser(user)
            }

        case .ACCOUNT_ADDED, .ACCOUNT_UPDATED:
            IKLogger.xpc.log("[KD] TODO - account signal")

        case .DRIVE_ADDED, .DRIVE_UPDATED:
            IKLogger.xpc.log("[KD] TODO - drive signal")

        default:
            IKLogger.xpc.error("[KD] recv error code:\(String(describing: signalMetadata.code)) cause:\(String(describing: signalMetadata.cause))")
        }
    }
}
