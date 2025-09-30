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

enum WindowRoute {
    case preloading
    case onboarding
    case splitView
}

final class MainWindowRouter: WindowRouter {
    @MainActor
    private lazy var mainWindowController: MainWindowController? = {
        let mainWindow = NSApplication.shared.windows.first { $0.windowController is MainWindowController }
        return mainWindow?.windowController as? MainWindowController
    }()

    @MainActor
    func navigate(to route: WindowRoute) {
        guard let mainWindowController else { return }

        switch route {
        case .preloading:
            let viewController = PreloadingViewController()
            mainWindowController.setViewController(viewController)
        case .onboarding:
            print("ToDo: Show Onboarding Controller")
        case .splitView:
            let viewController = MainSplitViewController()
            mainWindowController.setViewController(viewController)
        }
    }
}
