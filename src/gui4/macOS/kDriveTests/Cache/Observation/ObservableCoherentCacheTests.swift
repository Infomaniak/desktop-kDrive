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

@testable import kDriveCore
import OrderedCollections
import Testing

struct ObservableCoherentCacheTests {
    @Test(.timeLimit(.minutes(1)))
    func storesServerLevelErrorInServerErrors() async throws {
        // GIVEN
        let serverError = ObservableData.expectedServerError
        let cache = ServerCoherentCache()

        // WHEN
        try await cache.addOrUpdateError(serverError)

        // THEN
        let storedError = await cache.serverErrors[ObservableData.expectedServerErrorDbId]
        #expect(storedError != nil, "Server level error should be stored in serverErrors")
    }

    @Test(.timeLimit(.minutes(1)))
    func routesSyncPalUpdateRequiredErrorToServerErrors() async throws {
        // GIVEN
        let syncPalError = ObservableData.syncPalUpdateRequiredError
        let cache = ServerCoherentCache()

        // WHEN
        try await cache.addOrUpdateError(syncPalError)

        // THEN
        let storedError = await cache.serverErrors[ObservableData.syncPalUpdateRequiredErrorDbId]
        #expect(storedError != nil, "SyncPal level UpdateRequired error should be routed to serverErrors")
    }
}
