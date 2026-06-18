/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import Cocoa
import Combine
import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import kDriveResources
import OrderedCollections
import SwiftUI

typealias UIIndexedSynchroInfo = OrderedDictionary<UISynchro.ID, UISynchroInfo>

extension UIIndexedSynchroInfo {
    init(indexedSynchro: [SynchroContext]) {
        self.init(
            uniqueKeysWithValues: indexedSynchro.map {
                let info = UISynchroInfo(
                    context: UISynchroContext(synchroContext: $0),
                    state: UISynchroState(fromSynchro: $0.synchro)
                )
                return (UISynchro.ID($0.synchro.dbId), info)
            }
        )
    }
}

struct UISynchroInfo: Equatable {
    let context: UISynchroContext
    let state: UISynchroState
}

extension SidebarItem {
    static let home = SidebarItem(
        icon: KDriveResources.house.image,
        title: KDriveLocalizable.tabTitleHome
    )
    static let activities = SidebarItem(
        icon: KDriveResources.circularArrowsClockwise.image,
        title: KDriveLocalizable.tabTitleActivities
    )
    static let storage = SidebarItem(
        icon: KDriveResources.hardDiskDrive.image,
        title: KDriveLocalizable.tabTitleStorage
    )
    static let openInFinder = SidebarItem(
        icon: KDriveResources.finder.image,
        title: KDriveLocalizable.buttonOpenInFinder,
        type: .action
    )

    static let mainViewItems: [SidebarItem] = [.home, .activities, .storage, .openInFinder]
}

final class MainSidebarViewController: NSViewController {
    static let navigationCellIdentifier = NSUserInterfaceItemIdentifier(String(describing: SidebarTableCellView.self))

    @LazyInjectService private var router: MainViewRouter
    @LazyInjectService private var loadingIndicatorShower: SidebarNotificationPresenting
    @LazyInjectService private var observableCache: CoherentCacheObservable
    @LazyInjectService private var coherentCache: CoherentCache

    private let mainViewModel: MainViewModel
    private var bindStore = Set<AnyCancellable>()

    private let items = SidebarItem.mainViewItems

    private var hasBlockingError = false

    private var synchroInfos = UIIndexedSynchroInfo()
    private var currentSynchroId: UISynchro.ID?
    private var activitiesHasError = false

    private lazy var sidebarNotificationView: SidebarNotificationView = {
        let view = SidebarNotificationView()
        view.translatesAutoresizingMaskIntoConstraints = false
        view.configure(with: nil)
        return view
    }()

    private lazy var scrollView: NSScrollView = {
        let scrollView = NSScrollView()
        scrollView.translatesAutoresizingMaskIntoConstraints = false
        scrollView.hasVerticalScroller = true
        scrollView.autohidesScrollers = true
        scrollView.drawsBackground = false

        return scrollView
    }()

    private lazy var outlineView: NSOutlineView = {
        let outlineView = ClickableOutlineView()
        outlineView.translatesAutoresizingMaskIntoConstraints = false
        outlineView.dataSource = self
        outlineView.delegate = self
        outlineView.focusRingType = .none
        outlineView.rowSizeStyle = .medium
        outlineView.headerView = nil
        outlineView.style = .sourceList

        return outlineView
    }()

    private let synchroSelectorViewModel = SynchroSelectorViewModel()

    private lazy var synchroSelectorView: NSHostingView<SynchroSelectorView> = {
        let hostingView = NSHostingView(rootView: SynchroSelectorView(viewModel: synchroSelectorViewModel))
        hostingView.translatesAutoresizingMaskIntoConstraints = false
        hostingView.setContentHuggingPriority(.defaultHigh, for: .vertical)
        return hostingView
    }()

    init(mainViewModel: MainViewModel) {
        self.mainViewModel = mainViewModel
        super.init(nibName: nil, bundle: nil)
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func viewDidLoad() {
        super.viewDidLoad()

        setupView()
        bindViewModel()
        fetchInitialSynchros()
    }

    override func viewWillAppear() {
        super.viewWillAppear()

        outlineView.selectRowIndexes(IndexSet(integer: 0), byExtendingSelection: false)
    }

    override func viewDidLayout() {
        super.viewDidLayout()
        updateScrollViewElasticity()
    }

    private func bindViewModel() {
        observableCache.usersPublisher.allSynchrosPublisher()
            .map { synchroContext in
                let sortedSynchroContext = synchroContext.sorted { lhs, rhs in
                    if lhs.drive.name.localizedCaseInsensitiveCompare(rhs.drive.name) == .orderedAscending {
                        return true
                    }
                    return lhs.synchro.targetNodeId.isEmpty && !rhs.synchro.targetNodeId.isEmpty
                }
                return UIIndexedSynchroInfo(indexedSynchro: sortedSynchroContext)
            }
            .removeDuplicates()
            .eraseToAnyPublisher()
            .receiveOnMain(store: &bindStore) { [weak self] synchroContexts in
                self?.updateSynchrosList(synchroContexts)
                self?.updateSidebarIfNecessary()
            }

        loadingIndicatorShower.statePublisher
            .receive(on: RunLoop.main)
            .throttle(for: 0.5, scheduler: RunLoop.main, latest: true)
            .sink { [weak self] state in
                self?.sidebarNotificationView.configure(with: state)
            }
            .store(in: &bindStore)

        router.$currentPath
            .receiveOnMain(store: &bindStore) { [weak self] path in
                self?.updateSelectedItemIfNecessary(path)
            }

        mainViewModel.currentSynchroContextPublisher
            .receiveOnMain(store: &bindStore) { [weak self] synchroContext in
                self?.synchroSelectorViewModel.selectedSynchroId = synchroContext?.synchro.id
                self?.currentSynchroId = synchroContext?.synchro.id
                self?.updateActivitiesBadge()
            }
    }

    private func fetchInitialSynchros() {
        Task {
            let synchroContexts = await coherentCache.getSynchroContexts()
            let uiSynchroInfo = UIIndexedSynchroInfo(indexedSynchro: synchroContexts)

            updateSynchrosList(uiSynchroInfo)
        }
    }

    private func setupView() {
        let headerView = SidebarHeaderView()
        headerView.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(headerView)

        synchroSelectorViewModel.onSelect = { [weak self] synchro in
            self?.mainViewModel.setCurrentSynchro(synchro)
        }
        view.addSubview(synchroSelectorView)
        setupScrollAndOutlineView()
        view.addSubview(sidebarNotificationView)

        NSLayoutConstraint.activate([
            headerView.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            headerView.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: AppPadding.padding16),
            headerView.trailingAnchor.constraint(
                equalTo: view.safeAreaLayoutGuide.trailingAnchor,
                constant: -AppPadding.padding16
            ),

            synchroSelectorView.topAnchor.constraint(equalTo: headerView.bottomAnchor, constant: AppPadding.padding16),
            synchroSelectorView.leadingAnchor.constraint(
                equalTo: view.safeAreaLayoutGuide.leadingAnchor,
                constant: AppPadding.padding12
            ),
            synchroSelectorView.trailingAnchor.constraint(
                equalTo: view.safeAreaLayoutGuide.trailingAnchor,
                constant: -AppPadding.padding12
            ),

            scrollView.topAnchor.constraint(equalTo: synchroSelectorView.bottomAnchor, constant: AppPadding.padding16),
            scrollView.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor),
            scrollView.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor),

            sidebarNotificationView.topAnchor.constraint(equalTo: scrollView.bottomAnchor, constant: AppPadding.padding16),
            sidebarNotificationView.leadingAnchor.constraint(
                equalTo: view.safeAreaLayoutGuide.leadingAnchor,
                constant: AppPadding.padding16
            ),
            sidebarNotificationView.trailingAnchor.constraint(
                equalTo: view.safeAreaLayoutGuide.trailingAnchor,
                constant: -AppPadding.padding16
            ),
            sidebarNotificationView.bottomAnchor.constraint(
                equalTo: view.safeAreaLayoutGuide.bottomAnchor,
                constant: -AppPadding.padding16
            )
        ])
    }

    private func setupScrollAndOutlineView() {
        let singleColumn = NSTableColumn(identifier: NSUserInterfaceItemIdentifier("SidebarColumn"))
        singleColumn.isEditable = false
        outlineView.addTableColumn(singleColumn)
        outlineView.outlineTableColumn = singleColumn

        scrollView.documentView = outlineView
        view.addSubview(scrollView)
    }

    private func updateScrollViewElasticity() {
        guard let documentView = scrollView.documentView else { return }

        let isDocumentViewSmallerThanScrollView = documentView.bounds.height <= scrollView.documentVisibleRect.height
        scrollView.verticalScrollElasticity = isDocumentViewSmallerThanScrollView ? .none : .automatic
    }

    private func openSyncInFolder() {
        guard let currentSynchro = mainViewModel.currentSynchro else { return }
        NSWorkspace.shared.open(currentSynchro.localPath)
    }

    private func updateSynchrosList(_ synchroInfo: UIIndexedSynchroInfo) {
        synchroInfos = synchroInfo
        synchroSelectorViewModel.update(with: Array(synchroInfo.values))
        updateActivitiesBadge()
    }

    private func updateActivitiesBadge() {
        let hasError: Bool
        if let currentSynchroId, let info = synchroInfos[currentSynchroId] {
            hasError = info.state.errorCount > 0
        } else {
            hasError = false
        }

        guard activitiesHasError != hasError else { return }
        activitiesHasError = hasError

        guard let activitiesIndex = items.firstIndex(of: .activities),
              let cell = outlineView.view(
                  atColumn: 0,
                  row: activitiesIndex,
                  makeIfNecessary: false
              ) as? SidebarTableCellView else {
            return
        }

        cell.showsBadge = hasError
    }

    private func updateSidebarIfNecessary() {
        let shouldShowBlockingError = mainViewModel.currentBlockingError != nil
        guard hasBlockingError != shouldShowBlockingError else {
            return
        }

        let previousSelectedRow = outlineView.selectedRow == -1 ? 0 : outlineView.selectedRow
        outlineView.reloadData()

        if shouldShowBlockingError {
            outlineView.selectRowIndexes([], byExtendingSelection: false)
        } else {
            outlineView.selectRowIndexes([previousSelectedRow], byExtendingSelection: false)
        }

        hasBlockingError = shouldShowBlockingError
    }

    private func updateSelectedItemIfNecessary(_ path: Path<MainViewTab>) {
        let tab = path.mainTab
        guard let tabIndex = SidebarItem.mainViewItems.compactMap(\.mainViewTab).firstIndex(of: tab) else {
            return
        }

        outlineView.selectRowIndexes(IndexSet(integer: tabIndex), byExtendingSelection: false)
    }
}

// MARK: - NSOutlineViewDataSource

extension MainSidebarViewController: NSOutlineViewDataSource {
    func outlineView(_ outlineView: NSOutlineView, child index: Int, ofItem item: Any?) -> Any {
        return items[index]
    }

    func outlineView(_ outlineView: NSOutlineView, isItemExpandable item: Any) -> Bool {
        return false
    }

    func outlineView(_ outlineView: NSOutlineView, numberOfChildrenOfItem item: Any?) -> Int {
        guard item == nil else { return 0 }
        return items.count
    }
}

// MARK: - OutlineViewDelegate

extension MainSidebarViewController: ClickableOutlineViewDelegate {
    func outlineView(_ outlineView: NSOutlineView, shouldSelectItem item: Any) -> Bool {
        guard let item = item as? SidebarItem else { return false }
        return item.canBeSelected && mainViewModel.currentBlockingError == nil
    }

    func outlineView(_ outlineView: NSOutlineView, viewFor tableColumn: NSTableColumn?, item: Any) -> NSView? {
        guard let item = item as? SidebarItem else { return nil }

        var cell = outlineView.makeView(withIdentifier: Self.navigationCellIdentifier, owner: self) as? SidebarTableCellView
        if cell == nil {
            cell = SidebarTableCellView()
            cell?.identifier = Self.navigationCellIdentifier
        }

        let enabled = !item.canBeSelected || mainViewModel.currentBlockingError == nil
        cell?.setupForItem(item, enabled: enabled)
        cell?.showsBadge = item == .activities && activitiesHasError
        return cell
    }

    func outlineViewSelectionDidChange(_ notification: Notification) {
        guard let selectedItem = outlineView.item(atRow: outlineView.selectedRow) as? SidebarItem,
              let path = selectedItem.mainViewTab else {
            return
        }

        router.setCurrentTabIfNecessary(path)
    }

    func outlineView(_: NSOutlineView, didClick item: Any?) {
        guard let item = item as? SidebarItem, item.type == .action else {
            return
        }

        switch item {
        case .openInFinder:
            openSyncInFolder()
        default:
            break
        }
    }
}
