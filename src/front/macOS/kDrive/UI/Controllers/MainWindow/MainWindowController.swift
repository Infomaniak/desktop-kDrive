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
                let route = await guessBestRouteWhenXPCIsConnected()
                router.navigate(to: route)
            }
        }
    }

    private func guessBestRouteWhenXPCIsConnected() async -> WindowRoute {
        guard let connectedUsers = try? await UserJobs().userInfoList().filter({ $0.isConnected }) else {
            return .onboarding()
        }

        let hasConnectedUser = connectedUsers.isEmpty == false

        if UserDefaults.standard.isFirstLaunch {
            UserDefaults.standard.shouldPresentOnboarding = !hasConnectedUser
            UserDefaults.standard.isFirstLaunch = false
        }

        if hasConnectedUser && !UserDefaults.standard.shouldPresentOnboarding {
            return .mainWindow()
        } else {
            return .onboarding()
        }
    }

    private func onRouteChange(route: WindowRoute) {
        switch route {
        case .preloading(let isShowingError):
            setViewController(PreloadingViewController(isShowingError: isShowingError))
        case .onboarding(let user, let onboardingStep):
            setViewController(OnboardingViewController(user: user, initialStep: onboardingStep))
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
}
