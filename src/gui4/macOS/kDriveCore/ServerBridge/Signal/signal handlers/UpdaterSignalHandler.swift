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

struct UpdaterSignalHandler {
    private let decoder = JSONDecoder()
    @LazyInjectService private var updaterCache: UpdaterCache

    func handleShowDialog(_ signal: Data) async throws {
        guard let showDialogSignal = try? decoder.decode(SignalMessage<UpdaterShowDialogSignal>.self, from: signal) else {
            throw SignalError.unableToGetVersionInfoFromSignal
        }

        let versionInfo = showDialogSignal.body.versionInfo

        IKLogger.xpc.log("[KD] Update available: \(versionInfo.tag) (build: \(versionInfo.buildVersion))")

        // TODO: Show updater UI via AppRouter state change
    }

    func handleStateChanged(_ signal: Data) async throws {
        guard let stateChangedSignal = try? decoder.decode(SignalMessage<UpdaterStateChangedSignal>.self, from: signal) else {
            throw SignalError.unableToGetUpdateStateFromSignal
        }

        let updateState = stateChangedSignal.body.updateState

        IKLogger.xpc.log("[KD] Updater state changed: \(updateState)")

        await updaterCache.setUpdateState(updateState)
    }
}
