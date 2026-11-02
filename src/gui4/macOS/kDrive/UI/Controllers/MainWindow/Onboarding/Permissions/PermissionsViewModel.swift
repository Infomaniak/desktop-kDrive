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

enum MacOSPermissionState: Sendable {
    case neutral
    case warning
    case done
}

@MainActor
final class PermissionsViewModel: ObservableObject {
    static let requiredPermissions: [MacOSPermission] = [.endpointSecurityExtension, .fullDiskAccess]

    @LazyInjectService private var permissionHander: MacOSPermissionHandling

    @Published var currentPermission: MacOSPermission
    @Published var currentState: MacOSPermissionState = .neutral

    private let flowCoordinator: OnboardingFlowCoordinator

    private var bindStore = Set<AnyCancellable>()
    private var didRequestExtensionInstall = false
    private var refreshTask: Task<Void, Never>?
    private var permissionCheckTask: Task<Void, Never>?
    private var extensionInstallTask: Task<Void, Never>?

    init(flowCoordinator: OnboardingFlowCoordinator) {
        self.flowCoordinator = flowCoordinator

        guard case .permissions(let permission) = flowCoordinator.currentStep else {
            currentPermission = PermissionsViewModel.requiredPermissions[0]
            return
        }
        currentPermission = permission

        flowCoordinator.$currentStep
            .receiveOnMain(store: &bindStore) { [weak self] newStep in
                self?.updateCurrentPermission(from: newStep)
            }
    }

    deinit {
        refreshTask?.cancel()
        permissionCheckTask?.cancel()
        extensionInstallTask?.cancel()
    }

    func startRefreshing() {
        guard refreshTask == nil else { return }

        refreshTask = Task { [weak self] in
            while !Task.isCancelled {
                do {
                    try await Task.sleep(nanoseconds: 2_000_000_000)
                } catch {
                    return
                }
                guard !Task.isCancelled else { return }
                self?.updatePermissionStatus()
            }
        }
        updatePermissionStatus()
        installLiteSyncExtensionIfNeeded()
    }

    func stopRefreshing() {
        refreshTask?.cancel()
        refreshTask = nil
        permissionCheckTask?.cancel()
        permissionCheckTask = nil
        extensionInstallTask?.cancel()
        extensionInstallTask = nil
    }

    func updatePermissionStatus() {
        guard refreshTask != nil, permissionCheckTask == nil else { return }
        let permission = currentPermission
        let permissionHandler = permissionHander
        permissionCheckTask = Task { [weak self] in
            guard !Task.isCancelled else { return }
            let isAuthorized = await permissionHandler.isAuthorized(for: permission)
            // The XPC request may still reply after its waiting task has been cancelled.
            guard !Task.isCancelled, let self, refreshTask != nil, currentPermission == permission else { return }
            currentState = isAuthorized ? .done : .warning
            permissionCheckTask = nil
        }
    }

    private func installLiteSyncExtensionIfNeeded() {
        guard case .endpointSecurityExtension = currentPermission else { return }
        guard !didRequestExtensionInstall else { return }
        didRequestExtensionInstall = true

        extensionInstallTask = Task { [weak self] in
            guard !Task.isCancelled else { return }
            do {
                try await UtilityJobs().installLiteSyncExtension()
            } catch {
                IKLogger.general.error("Failed to install Lite Sync extension: \(error)")
                self?.didRequestExtensionInstall = false
            }
            guard !Task.isCancelled else { return }
            self?.updatePermissionStatus()
        }
    }

    func navigateIfPossible() {
        Task {
            guard currentState == .done else {
                return
            }
            await flowCoordinator.navigateToNextStepOrFinish()
        }
    }

    private func updateCurrentPermission(from step: OnboardingStep) {
        guard case .permissions(let permission) = step else { return }
        currentPermission = permission
    }
}
