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
import OrderedCollections

struct DriveSignalHandler {
    private let decoder = JSONDecoder()
    @LazyInjectService private var coherentCache: CoherentCache

    func handleDriveAddedOrUpdated(_ signal: Data) async throws {
        guard let driveInfoSignal = try? decoder.decode(SignalMessage<DriveInfoSignal>.self, from: signal) else {
            throw SignalError.unableToGetDriveFromSignal
        }

        let driveInfo = driveInfoSignal.body.driveInfo
        try await coherentCache.addOrUpdateDriveSignal(driveInfo)
    }

    func handleDriveRemoved(_ signal: Data) async throws {
        guard let driveInfoSignal = try? decoder.decode(SignalMessage<DriveRemoveSignal>.self, from: signal) else {
            throw SignalError.unableToGetDriveDbIdFromSignal
        }

        let driveDbId = driveInfoSignal.body.driveDbId
        try await coherentCache.removeDrive(driveDbId: driveDbId)
    }
}

extension CoherentCache {
    func addOrUpdateDriveSignal(_ driveSignal: DriveInfoSignalMetadata) async throws {
        let accountDbId = driveSignal.accountDbId
        guard var account = await getAccount(accountDbId: accountDbId) else {
            throw ServerCoherentCache.CacheError.accountNotFound(accountDbId)
        }

        let existingDrive = account.drives[driveSignal.dbId]
        let updatedDrive = driveSignal.asDrive(accountId: account.id,
                                               userDbId: account.userDbId,
                                               synchros: existingDrive?.synchros ?? [:])
        account.drives[driveSignal.dbId] = updatedDrive

        try await addOrUpdateAccount(account)
    }
}
