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
import InfomaniakConcurrency
import InfomaniakDI
import kDriveCore

@MainActor
final class DriveSelectionViewModel: ObservableObject {
    @LazyInjectService private var coherentCache: CoherentCache
    @LazyInjectService private var syncCreator: SyncCreator

    private let flowCoordinator: OnboardingFlowCoordinator

    @Published private(set) var isLoading = false

    @Published private(set) var availableDrives: [UIAvailableDrive]?
    @Published private(set) var selectedDrives = Set<UIAvailableDrive>()

    init(flowCoordinator: OnboardingFlowCoordinator) {
        self.flowCoordinator = flowCoordinator
    }

    func loadAvailableDrives() async throws {
        guard let targetUser = flowCoordinator.targetUser else {
            flowCoordinator.navigate(to: .login)
            return
        }

        let drivesResponse = try await DriveJobs().availableDrives(userDbId: Int32(targetUser.dbId))
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
        guard !selectedDrives.isEmpty, let targetUser = flowCoordinator.targetUser else { return }

        Task {
            isLoading = true

            do {
                // TODO: Use the cache property when available (next XPC PR)
                let availableDrives = try await DriveJobs().availableDrives(userDbId: Int32(targetUser.dbId)).asAvailableDrives

                try await selectedDrives.concurrentForEach { selectedDrive in
                    guard let availableDrive = availableDrives.first(where: { $0.id == selectedDrive.id }) else {
                        return
                    }

                    let syncOrigin = SyncOrigin.availableDrive(availableDrive)
                    let localFolder = try await self.syncCreator.preferredLocalPath(for: syncOrigin)

                    let newSyncCandidate = NewSyncCandidate(
                        origin: syncOrigin,
                        remoteFolder: .kDriveRoot,
                        localFolder: localFolder,
                        blackList: []
                    )

                    Task { @MainActor in
                        self.flowCoordinator.synchronizations.append(newSyncCandidate)
                    }
                }

                flowCoordinator.navigateToNextStep()
            } catch {
                // TODO: Handle error
                isLoading = false
            }
        }
    }
}
