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

@MainActor
final class PermissionsViewModel: ObservableObject {
    static let requiredPermissions: [MacOSPermission] = [.fullDiskAccess, .endpointSecurityExtension]

    @Published var currentPermission: MacOSPermission

    private let flowCoordinator: OnboardingFlowCoordinator

    private var bindStore = Set<AnyCancellable>()

    init(flowCoordinator: OnboardingFlowCoordinator) {
        self.flowCoordinator = flowCoordinator

        guard case .permissions(let permission) = flowCoordinator.currentStep else {
            currentPermission = .fullDiskAccess
            return
        }
        currentPermission = permission
        flowCoordinator.$currentStep
            .receiveOnMain(store: &bindStore) { [weak self] newStep in
                self?.updateCurrentPermission(from: newStep)
            }
    }

    private func updateCurrentPermission(from state: OnboardingStep) {
        guard case .permissions(let permission) = state else { return }
        currentPermission = permission
    }
}
