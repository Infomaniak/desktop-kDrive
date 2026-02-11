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

import Cocoa
import Combine
import Foundation
import InfomaniakConcurrency
import InfomaniakDI
import kDriveCore
import kDriveCoreUI

@MainActor
final class DriveSelectionViewModel: ObservableObject {
    @LazyInjectService private var syncCreator: SyncCreator

    @LazyInjectService private var coherentCache: CoherentCache
    @LazyInjectService private var coherentCacheObservable: CoherentCacheObservable

    private let flowCoordinator: OnboardingFlowCoordinator

    private var bindStore = Set<AnyCancellable>()

    @Published private(set) var isLoading = false

    @Published private(set) var availableDrives = [UIAvailableDrive]()
    @Published private(set) var selectedDrives = Set<UIAvailableDrive>()

    init(flowCoordinator: OnboardingFlowCoordinator) {
        self.flowCoordinator = flowCoordinator
        observeAvailableDrives()
    }

    private func observeAvailableDrives() {
        coherentCacheObservable.usersPublisher.allAvailableDrivesPublisher()
            .receiveOnMain(store: &bindStore) { [weak self] availableDriveContext in
                self?.availableDrives = availableDriveContext.map { UIAvailableDrive(availableDrive: $0.availableDrive) }
            }
    }

    func loadAvailableDrives() async throws {
        guard let user = flowCoordinator.currentUser else {
            flowCoordinator.navigate(to: .login)
            return
        }

        _ = try await DriveJobs().availableDrives(userDbId: Int32(user.dbId))
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
            isLoading = true

            do {
                let syncCandidates: [NewSyncCandidate] = try await selectedDrives.concurrentCompactMap { selectedDrive in
                    guard let uiAvailableDrive = await self.availableDrives.first(where: { $0.id == selectedDrive.id }),
                          let availableDrive = await self.coherentCache.getAvailableDrive(driveDb: Int32(uiAvailableDrive.id))
                    else {
                        return nil
                    }

                    let syncOrigin = SyncOrigin.availableDrive(availableDrive)
                    let localFolder = try await self.syncCreator.preferredLocalPath(for: syncOrigin)

                    let newSyncCandidate = NewSyncCandidate(
                        origin: syncOrigin,
                        remoteFolder: .kDriveRoot,
                        localFolder: localFolder,
                        blackList: []
                    )

                    return newSyncCandidate
                }

                flowCoordinator.synchronizations = syncCandidates
                await flowCoordinator.navigateToNextStep()
            } catch {
                // TODO: Handle error
                isLoading = false
            }
        }
    }
}
