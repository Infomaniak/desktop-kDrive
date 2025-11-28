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

import Combine
import Foundation
import InfomaniakDI
import kDriveCore

enum OnboardingStep: Sendable {
    case login
    case drivesSelection
    case permissions
    case synchronization
}

@MainActor
final class OnboardingFlowCoordinator: ObservableObject {
    @Published private(set) var currentStep: OnboardingStep

    @Published var targetUser: UIUser?

    var synchronizations = [NewSyncCandidate]()

    init(initialStep: OnboardingStep?) {
        currentStep = initialStep ?? .login
    }

    func navigate(to step: OnboardingStep) {
        currentStep = step
    }

    func navigateToNextStep() {
        guard let nextStep = nextRequiredStep(from: currentStep) else {
            return
        }

        navigate(to: nextStep)
    }

    func nextRequiredStep(from currentStep: OnboardingStep) -> OnboardingStep? {
        switch currentStep {
        case .login:
            return .drivesSelection
        case .drivesSelection:
            // TODO: Check if permissions are required
            return .permissions
        case .permissions:
            return .synchronization
        case .synchronization:
            return nil
        }
    }

    func guessAndNavigateToInitialStep() {
        Task {
            @InjectService var coherentCache: CoherentCache
            let user = await coherentCache.getFirstAvailableUser()

            guard let user else {
                return
            }

            targetUser = UIUser(user: user)
            navigateToNextStep()
        }
    }
}
