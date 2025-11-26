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

    @Published private(set) var currentUser: UIUser?
    @Published private(set) var availableDrives: [UIAvailableDrive]?
    @Published private(set) var selectedDrives = Set<UIAvailableDrive>()

    init() {}

    func loadAvailableDrives() async throws {
        guard let firstAvailableUser = await coherentCache.getFirstAvailableUser() else {
            return
        }

        currentUser = UIUser(user: firstAvailableUser)

        let drivesResponse = try await DriveJobs().availableDrives(userDbId: firstAvailableUser.dbId)
        let availableDrives = drivesResponse.asAvailableDrives
        self.availableDrives = availableDrives.map { UIAvailableDrive(availableDrive: $0) }
    }

    func toggleDriveSelection(_ drive: UIAvailableDrive) {
        if selectedDrives.contains(drive) {
            selectedDrives.remove(drive)
        } else {
            selectedDrives.insert(drive)
        }
    }

    func startSynchronization() {
        guard !selectedDrives.isEmpty else { return }

        Task {
            guard let currentUser else {
                return
            }

            let drives = try await DriveJobs().availableDrives(userDbId: Int32(currentUser.dbId)).asAvailableDrives

            for selectedDrive in selectedDrives {
                guard let drive = drives.first(where: { $0.driveId == selectedDrive.driveId }) else {
                    continue
                }

                let identifier = NewSyncParentIdentifier.transitive(
                    userDbId: drive.userDbId,
                    accountId: drive.accountId,
                    driveId: drive.driveId
                )
                let metadata = NewSyncMetadata(
                    userDbId: drive.userDbId,
                    accountId: drive.accountId,
                    driveId: drive.driveId,
                    localFolderPath: "/Users/valentinperignon/kDriveTest",
                    serverFolderPath: "/",
                    serverFolderNodeId: "1",
                    liteSync: true,
                    blackList: [],
                    whiteList: []
                )

                do {
                    let syncInfo = try await SyncJobs().addSync(identifier: identifier, metadata: metadata)
                    print(syncInfo)

                    try await SyncJobs().startSync(syncDbId: syncInfo.dbId)
                } catch {
                    print("Coucou")
                }
            }
        }
    }
}
