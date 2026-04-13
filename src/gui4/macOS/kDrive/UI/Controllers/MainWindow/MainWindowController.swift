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
import InfomaniakDI
import kDriveCore

final class MainWindowController: NSWindowController {
    enum WindowConstants {
        static let frameName = "kDriveMainWindow"
        static let initialSize = NSRect(origin: .zero, size: CGSize(width: 900, height: 600))
        static let properties: NSWindow.StyleMask = [.titled, .closable, .resizable, .miniaturizable, .fullSizeContentView]
    }

    @LazyInjectService private var router: MainWindowRouter
    @LazyInjectService private var xpcConnectionProvider: XPCConnectionProvider
    @LazyInjectService private var coherentCache: CoherentCache

    // periphery:ignore - We keep a strong reference on the viewController being presented
    private var viewController: NSViewController?
    private var bindStore = Set<AnyCancellable>()

    init() {
        let window = NSWindow(
            contentRect: WindowConstants.initialSize,
            styleMask: WindowConstants.properties,
            backing: .buffered,
            defer: true
        )
        super.init(window: window)

        window.center()
        window.setFrameAutosaveName(WindowConstants.frameName)
        window.minSize = NSSize(width: 800, height: 450)
        window.delegate = self

        observeRouter()
        observeXPConnectionState()
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    private func observeRouter() {
        router.$currentRoute
            .receiveOnMain(store: &bindStore) { [weak self] route in
                self?.onRouteChange(route: route)
            }
    }

    private func observeXPConnectionState() {
        xpcConnectionProvider.guiConnectionStatePublisher
            .receiveOnMain(store: &bindStore) { [weak self] state in
                self?.navigateAfterPreloading(state: state)
            }
    }

    private func navigateAfterPreloading(state: XPCConnectionState) {
        switch state {
        case .notConnected:
            router.navigate(to: .preloading(isShowingError: false))
        case .error:
            router.navigate(to: .preloading(isShowingError: true))
        case .connected:
            Task {
                guard await !presentPermissionsViewIfNecessary() else { return }

                let route = await guessBestRouteWhenXPCIsConnected()
                router.navigate(to: route)
            }
        }
    }

    private func guessBestRouteWhenXPCIsConnected() async -> WindowRoute {
        let hasConnectedUser = await coherentCache.getFirstAvailableUser() != nil

        if hasConnectedUser {
            return .mainWindow()
        } else {
            return .onboarding()
        }
    }

    private func onRouteChange(route: WindowRoute) {
        switch route {
        case .preloading(let isShowingError):
            setViewController(PreloadingViewController(isShowingError: isShowingError))
        case .onboarding(let user, let steps, let initialStep):
            setViewController(OnboardingViewController(user: user, steps: steps, initialStep: initialStep))
        case .mainWindow(let tab):
            setViewController(MainViewController())
            if let tab {
                @InjectService var mainViewRouter: MainViewRouter
                mainViewRouter.setCurrentTab(tab)
            }
        }
    }

    private func setViewController(_ viewController: NSViewController) {
        self.viewController = viewController
        window?.contentView = viewController.view
    }

    @discardableResult
    private func presentPermissionsViewIfNecessary() async -> Bool {
        #if DEBUG
        return false
        #else
        guard xpcConnectionProvider.guiConnectionState == .connected else {
            return false
        }

        @InjectService var windowRouter: MainWindowRouter
        if case .onboarding = windowRouter.currentRoute {
            return false
        }

        @InjectService var permissionHander: MacOSPermissionHandling
        var permissionsToShow = [OnboardingStep]()
        for requiredPermission in PermissionsViewModel.requiredPermissions {
            if await !permissionHander.isAuthorized(for: requiredPermission) {
                permissionsToShow.append(.permissions(requiredPermission))
            }
        }

        guard !permissionsToShow.isEmpty else {
            return false
        }
        windowRouter.navigate(to: .onboarding(nil, permissionsToShow, permissionsToShow.first))

        return true
        #endif
    }
}

// MARK: - NSWindowDelegate

extension MainWindowController: NSWindowDelegate {
    func windowDidBecomeMain(_ notification: Notification) {
        Task {
            await presentPermissionsViewIfNecessary()
        }
    }
}
