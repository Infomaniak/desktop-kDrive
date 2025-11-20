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
import kDriveCore

enum OnboardingStep: Sendable {
    case login
    case driveSelection
    case permissions
    case synchronisation
}

@MainActor
final class OnboardingFlowCoordinator: ObservableObject {
    @Published private(set) var currentStep: OnboardingStep = .login

    func navigateToNextStep(for user: User? = nil) {
        guard let user else {
            currentStep = .login
            return
        }

        // TODO: Check

        currentStep = .driveSelection
    }
}
