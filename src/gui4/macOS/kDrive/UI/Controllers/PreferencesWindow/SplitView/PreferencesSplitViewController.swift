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
import kDriveCoreUI

final class PreferencesSplitViewController: IKSplitViewController {
    static let sidebarWidth: CGFloat = 200

    @LazyInjectService private var router: PreferencesViewRouter

    private let viewModel = PreferencesViewModel()
    private let repository = PreferencesRepository()

    private var bindStore = Set<AnyCancellable>()

    override func viewDidLoad() {
        super.viewDidLoad()
        setupSplitView()
        bindViewModel()
    }

    override func viewWillAppear() {
        super.viewWillAppear()
        Task {
            async let _ = repository.refreshData()
            async let _ = viewModel.fetchInitialData()
        }
    }

    private func bindViewModel() {
        router.$currentPath
            .receiveOnMain(store: &bindStore) { [weak self] newPath in
                self?.onPathChange(newPath)
            }
        router.$currentModal
            .receiveOnMain(store: &bindStore) { [weak self] newPath in
                self?.onModalPathChange(newPath)
            }
    }

    private func setupSplitView() {
        splitView.autosaveName = "PreferencesSplitViewAutoSave"
        splitView.isVertical = true

        let sidebarViewController = PreferencesSidebarViewController()
        sidebarViewController.delegate = self
        let sidebarItem = NSSplitViewItem(sidebarWithViewController: sidebarViewController)
        sidebarItem.minimumThickness = Self.sidebarWidth
        sidebarItem.maximumThickness = Self.sidebarWidth
        addSplitViewItem(sidebarItem)

        let generalViewController = NSViewController()
        let contentItem = NSSplitViewItem(viewController: generalViewController)
        addSplitViewItem(contentItem)
    }

    func onModalPathChange(_ modalPath: ModalPath?) {
        if let modalPath {
            // TODO: Present some modal view controller based on modalPath
        } else if let presentedViewController = presentedViewControllers?.first {
            dismiss(presentedViewController)
        }
    }

    func onPathChange(_ path: PreferencesViewRouter.RouterPath) {
        let contentViewController: NSViewController
        switch path.details.last {
        case .general:
            contentViewController = GeneralPreferencesViewController(repository: repository)
        case .accounts:
            contentViewController = AccountsPreferencesViewController(viewModel: viewModel)
        case .advanced:
            contentViewController = AdvancedPreferencesViewController()
        case .syncedKDrive(let drive):
            contentViewController = SyncedKDrivePreferencesViewController(drive: drive)
        case .dataManagement:
            contentViewController = DataManagementPreferencesViewController()
        case .dataManagementDetail(let dataManagementItem):
            contentViewController = DataManagementPreferencesDetailViewController(dataManagementItem: dataManagementItem, repository: repository)
        default:
            contentViewController = GeneralPreferencesViewController(repository: repository)
        }

        switchContentViewController(destination: contentViewController)
    }
}

// MARK: - NavigableSidebarViewControllerDelegate

extension PreferencesSplitViewController: NavigableSidebarViewControllerDelegate {
    func sidebarViewController(_: NSViewController, didSelectItem item: kDriveCoreUI.SidebarItem) {
        guard let preferencesViewTab = item.preferencesViewTab else {
            return
        }

        router.setCurrentTab(preferencesViewTab)
    }
}
