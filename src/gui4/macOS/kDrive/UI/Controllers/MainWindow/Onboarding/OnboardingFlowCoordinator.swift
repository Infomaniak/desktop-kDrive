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
import kDriveCoreUI

enum OnboardingStep: Sendable, Equatable {
    case login
    case drivesSelection
    case permissions(MacOSPermission)
    case synchronization
    case appReady
}

@MainActor
final class OnboardingFlowCoordinator: ObservableObject {
    @Published var currentUser: UIUser?
    @Published private(set) var currentStep: OnboardingStep

    private let steps: [OnboardingStep] = [
        .login,
        .drivesSelection,
        .permissions(.endpointSecurityExtension),
        .permissions(.fullDiskAccess),
        .synchronization,
        .appReady
    ]

    var synchronizations = [NewSyncCandidate]()

    init(user: UIUser?, initialStep: OnboardingStep?) {
        currentUser = user
        currentStep = initialStep ?? .login
    }

    func guessAndNavigateToInitialStep() {
        Task {
            @InjectService var coherentCache: CoherentCache
            let user = await coherentCache.getFirstAvailableUser()

            guard let user else {
                return
            }

            currentUser = UIUser(user: user)
            await navigateToNextStepOrFinish()
        }
    }

    func navigate(to step: OnboardingStep) {
        currentStep = step
    }

    func navigateToNextStepOrFinish() async {
        guard let nextStep = await nextRequiredStep(from: currentStep) else {
            didFinishOnboarding()
            return
        }

        navigate(to: nextStep)
    }

    private func didFinishOnboarding() {
        @InjectService var windowRouter: MainWindowRouter
        windowRouter.navigate(to: .mainWindow())
    }

    private func nextRequiredStep(from currentStep: OnboardingStep) async -> OnboardingStep? {
        guard let indexOfCurrentStep = steps.firstIndex(of: currentStep), indexOfCurrentStep + 1 < steps.endIndex else {
            return nil
        }

        let remainingSteps = steps[(indexOfCurrentStep + 1)...]
        for step in remainingSteps {
            if await shouldNavigate(toStep: step) {
                return step
            }
        }

        return nil
    }

    private func shouldNavigate(toStep step: OnboardingStep) async -> Bool {
        switch step {
        case .login:
            return currentUser == nil
        case .drivesSelection:
            return true
        case .permissions(let macOSPermission):
            @InjectService var permissionHandler: MacOSPermissionHandling
            return await !permissionHandler.isAuthorized(for: macOSPermission)
        case .synchronization:
            return !synchronizations.isEmpty
        case .appReady:
            return true
        }
    }
}
