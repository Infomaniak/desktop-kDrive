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
    @Published private(set) var selectedDrives = Set<UIAvailableDrive>()

    @Published private(set) var availableDrives = [UIAvailableDrive]()
    var synchroConfigurations = [UIAvailableDrive.ID: SynchroConfiguration]()

    var selectedSynchroConfigurations: [SynchroConfiguration] {
        return synchroConfigurations
            .filter { indexedSynchroConfiguration in
                selectedDrives.contains { $0.id == indexedSynchroConfiguration.key }
            }
            .map { $0.value }
            .sorted {
                $0.drive.name.localizedCaseInsensitiveCompare($1.drive.name) == .orderedAscending
            }
    }

    init(flowCoordinator: OnboardingFlowCoordinator) {
        self.flowCoordinator = flowCoordinator
        observeAvailableDrives()
    }

    private func observeAvailableDrives() {
        coherentCacheObservable.usersPublisher.allAvailableDrivesPublisher()
            .map { $0.map { UIAvailableDrive(availableDrive: $0.availableDrive) } }
            .receiveOnMain(store: &bindStore) { [weak self] availableDrives in
                self?.availableDrives = availableDrives.sorted {
                    return $0.name.localizedCaseInsensitiveCompare($1.name) == .orderedAscending
                }
                self?.generateConfigurations(for: availableDrives)
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
                    else { return nil }

                    let syncOrigin = SyncOrigin.availableDrive(availableDrive)
                    let synchroConfiguration = await self.synchroConfigurations[selectedDrive.id]

                    let localFolder: URL
                    if let setupSynchroConfigurationURL = synchroConfiguration?.localFolder.url {
                        localFolder = setupSynchroConfigurationURL
                    } else {
                        localFolder = try await self.syncCreator.preferredLocalPath(for: syncOrigin.drive.name)
                    }

                    return NewSyncCandidate(
                        origin: syncOrigin,
                        remoteFolder: .kDriveRoot,
                        localFolder: localFolder,
                        blackList: synchroConfiguration?.blackList ?? []
                    )
                }

                flowCoordinator.synchronizations = syncCandidates
                await flowCoordinator.navigateToNextStepOrFinish()
            } catch {
                // TODO: Handle error
                isLoading = false
            }
        }
    }

    private func generateConfigurations(for drives: [UIAvailableDrive]) {
        for drive in drives {
            guard synchroConfigurations[drive.id] == nil else { continue }

            let configuration = SynchroConfiguration(drive: drive, blackList: [])
            synchroConfigurations[drive.id] = configuration
        }
    }
}
