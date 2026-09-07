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

import Foundation
import InfomaniakDI

struct UserSignalHandler {
    private let decoder = JSONDecoder()
    @LazyInjectService private var coherentCache: CoherentCache

    func handleUser(_ signal: Data) async throws {
        guard let userInfoSignal = try? decoder.decode(SignalMessage<UserInfoSignal>.self, from: signal) else {
            throw SignalError.unableToGetUserFromSignal
        }

        let user = User(userInfoMetadata: userInfoSignal.body.userInfo)
        await coherentCache.updateUser(user, updateOptions: .updateSignal)
    }

    func handleUserRemoved(_ signal: Data) async throws {
        guard let userRemoveSignal = try? decoder.decode(SignalMessage<UserRemoveSignal>.self, from: signal) else {
            throw SignalError.unableToGetUserDbIdFromSignal
        }

        let userDbId = userRemoveSignal.body.userDbId
        await coherentCache.removeUser(dbId: userDbId)
    }
}
