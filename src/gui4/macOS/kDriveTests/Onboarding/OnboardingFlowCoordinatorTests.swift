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

import Foundation
@testable import kDrive
import kDriveCore
import Testing

@MainActor
struct OnboardingFlowCoordinatorTests {
    @Test()
    func permissionsFirstStepsWithoutLoggedInUserProposesLoginAfterPermissions() {
        // WHEN
        let steps = OnboardingFlowCoordinator.permissionsFirstSteps(
            missingPermissions: [.endpointSecurityExtension, .fullDiskAccess],
            hasLoggedInUser: false
        )

        // THEN
        #expect(steps == [
            .permissions(.endpointSecurityExtension),
            .permissions(.fullDiskAccess),
            .login,
            .drivesSelection,
            .synchronization,
            .appReady
        ])
    }

    @Test()
    func permissionsFirstStepsWithSingleMissingPermissionAndNoUserProposesLogin() {
        // WHEN
        let steps = OnboardingFlowCoordinator.permissionsFirstSteps(
            missingPermissions: [.endpointSecurityExtension],
            hasLoggedInUser: false
        )

        // THEN
        #expect(steps == [
            .permissions(.endpointSecurityExtension),
            .login,
            .drivesSelection,
            .synchronization,
            .appReady
        ])
    }

    @Test()
    func permissionsFirstStepsWithLoggedInUserOnlyShowsPermissions() {
        // WHEN
        let steps = OnboardingFlowCoordinator.permissionsFirstSteps(
            missingPermissions: [.endpointSecurityExtension, .fullDiskAccess],
            hasLoggedInUser: true
        )

        // THEN
        #expect(steps == [
            .permissions(.endpointSecurityExtension),
            .permissions(.fullDiskAccess)
        ])
    }

    @Test()
    func permissionsFirstStepsWithNoMissingPermissionAndNoUserProposesLoginOnly() {
        // WHEN
        let steps = OnboardingFlowCoordinator.permissionsFirstSteps(
            missingPermissions: [],
            hasLoggedInUser: false
        )

        // THEN
        #expect(steps == [.login, .drivesSelection, .synchronization, .appReady])
    }

    @Test()
    func permissionsFirstStepsWithNoMissingPermissionAndLoggedInUserIsEmpty() {
        // WHEN
        let steps = OnboardingFlowCoordinator.permissionsFirstSteps(
            missingPermissions: [],
            hasLoggedInUser: true
        )

        // THEN
        #expect(steps.isEmpty)
    }
}
