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

import Combine
import Foundation
import InfomaniakDI
import kDriveCore

@MainActor
final class SynchronizationViewModel: ObservableObject {
    @LazyInjectService private var syncCreator: SyncCreator

    @Published var isShowingError = false

    private let flowCoordinator: OnboardingFlowCoordinator

    init(flowCoordinator: OnboardingFlowCoordinator) {
        self.flowCoordinator = flowCoordinator
    }

    func createSynchronizations() {
        Task {
            let syncCandidates = flowCoordinator.synchronizations
            var createdCount = 0
            IKLogger.general.info("[KD] Onboarding sync creation started requested=\(syncCandidates.count)")
            do {
                for syncCandidate in syncCandidates {
                    let syncInfo = try await syncCreator.create(from: syncCandidate)
                    createdCount += 1
                    IKLogger.general.info("[KD] Onboarding sync created syncDbId=\(syncInfo.dbId) driveDbId=\(syncInfo.driveDbId)")
                }
                IKLogger.general.info("[KD] Onboarding sync creation completed created=\(createdCount)")
            } catch {
                IKLogger.general.error(
                    "[KD] Onboarding sync creation failed created=\(createdCount) remaining=\(syncCandidates.count - createdCount) advancing=true"
                )
                isShowingError = true
            }

            await flowCoordinator.navigateToNextStepOrFinish()
        }
    }
}
