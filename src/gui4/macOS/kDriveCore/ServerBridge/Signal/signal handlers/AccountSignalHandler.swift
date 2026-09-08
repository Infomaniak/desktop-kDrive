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

struct AccountSignalHandler {
    private let decoder = JSONDecoder()
    @LazyInjectService private var coherentCache: CoherentCache

    func handleAccountAddedOrUpdated(_ signal: Data) async throws {
        guard let accountInfoSignal = try? decoder.decode(SignalMessage<AccountInfoSignal>.self, from: signal) else {
            throw SignalError.unableToGetAccountFromSignal
        }

        let accountInfo = accountInfoSignal.body.accountInfo
        try await coherentCache.addOrUpdateAccount(Account(with: accountInfo))
    }

    func handleAccountRemoved(_ signal: Data) async throws {
        guard let accountInfoSignal = try? decoder.decode(SignalMessage<AccountRemoveSignal>.self, from: signal) else {
            throw SignalError.unableToGetAccountDbIdFromSignal
        }

        let accountDbId = accountInfoSignal.body.accountDbId
        await coherentCache.removeAccount(accountDbId: accountDbId)
    }
}
