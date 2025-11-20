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

import AppKit
import Combine
import Foundation
import InfomaniakDI
import kDriveCore

@MainActor
final class DriveSelectionViewModel: ObservableObject {
    @LazyInjectService private var coherentCache: CoherentCache

    @Published private(set) var currentUser: User?
    @Published private(set) var availableDrives: [UIDrive]? = nil

    init() {}

    func loadAvailableDrives() async throws {
        currentUser = await coherentCache.getFirstAvailableUser()
        guard let currentUser else {
            return
        }

        // FIXME: We do not receive the drives from the XPC service yet. There is an async issue.
//        let drivesResponse = try await DriveJobs().availableDrives(userDbId: currentUser.dbId)
//        let availableDrives = drivesResponse.asDrives()
//        self.availableDrives = availableDrives.map { UIDrive(drive: $0) }

        availableDrives = [UIDrive(id: 1, name: "kDrive du Bojo", color: .blue)]
    }
}
