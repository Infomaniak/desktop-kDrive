/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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
}

final class MainSidebarViewController: NSViewController {
    static let navigationCellIdentifier = NSUserInterfaceItemIdentifier(String(describing: SidebarTableCellView.self))

    @LazyInjectService private var router: MainViewRouter
    @LazyInjectService private var loadingIndicatorShower: SidebarNotificationPresenting

    private let mainViewModel: MainViewModel
    private var bindStore = Set<AnyCancellable>()

    private let items: [SidebarItem] = [.home, .activities, .storage, .openInFinder]

    private lazy var sidebarNotificationView: SidebarNotificationView = {
        let view = SidebarNotificationView()
        view.translatesAutoresizingMaskIntoConstraints = false
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

    private lazy var popUpButton: ColoredPopUpButton = {
        let popUpButton = ColoredPopUpButton()
        popUpButton.translatesAutoresizingMaskIntoConstraints = false
        popUpButton.target = self
        popUpButton.action = #selector(didSelectSynchro)

        return popUpButton
    }()

    init(mainViewModel: MainViewModel) {
        self.mainViewModel = mainViewModel
        super.init(nibName: nil, bundle: nil)
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    deinit {
        // Break retain cycles by clearing delegate and target references
        outlineView.delegate = nil
        outlineView.dataSource = nil
        popUpButton.target = nil
    }

    override func viewDidLoad() {
        super.viewDidLoad()

        setupView()
        bindViewModel()
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
        mainViewModel.$availableSynchros
            .receiveOnMain(store: &bindStore) { [weak self] synchrosContext in
                self?.updateSynchrosList(synchrosContext)
                self?.updateSidebar()
            }

        loadingIndicatorShower.statePublisher
            .receive(on: RunLoop.main)
            .throttle(for: 0.5, scheduler: RunLoop.main, latest: true)
            .sink { [weak self] state in
                self?.sidebarNotificationView.configure(with: state)
            }
            .store(in: &bindStore)
    }

    private func setupView() {
        let headerView = SidebarHeaderView()
        headerView.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(headerView)

        view.addSubview(popUpButton)
        setupScrollAndOutlineView()
        view.addSubview(sidebarNotificationView)

        NSLayoutConstraint.activate([
            headerView.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            headerView.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: AppPadding.padding16),
            headerView.trailingAnchor.constraint(
                equalTo: view.safeAreaLayoutGuide.trailingAnchor,
                constant: -AppPadding.padding16
            ),

            popUpButton.topAnchor.constraint(equalTo: headerView.bottomAnchor, constant: AppPadding.padding16),
            popUpButton.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: AppPadding.padding16),
            popUpButton.trailingAnchor.constraint(
                equalTo: view.safeAreaLayoutGuide.trailingAnchor,
                constant: -AppPadding.padding16
            ),

            scrollView.topAnchor.constraint(equalTo: popUpButton.bottomAnchor, constant: AppPadding.padding16),
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

    @objc func didSelectSynchro() {
        guard let selectedItem = popUpButton.selectedItem,
              let synchro = selectedItem.representedObject as? UISynchro else {
            return
        }

        mainViewModel.setCurrentSynchro(synchro)
    }

    private func openSyncInFolder() {
        guard let currentSynchro = mainViewModel.currentSynchro else { return }
        NSWorkspace.shared.open(currentSynchro.localPath)
    }

    private func updateSynchrosList(_ syncs: [UISynchroContext]) {
        popUpButton.removeAllItems()
        for syncContext in syncs {
            let displayPath = !(syncContext.drive.synchros.count == 1)
            addPopUpItem(forSynchro: syncContext.synchro,
                         drive: syncContext.drive,
                         displaySynchroPath: displayPath)
        }
    }

    private func updateSidebar() {
        let previousSelectedRow = outlineView.selectedRow == -1 ? 0 : outlineView.selectedRow
        outlineView.reloadData()
        if mainViewModel.currentBlockingError != nil {
            outlineView.selectRowIndexes([], byExtendingSelection: false)
        } else {
            outlineView.selectRowIndexes([previousSelectedRow], byExtendingSelection: false)
        }
    }

    private func addPopUpItem(forSynchro synchro: UISynchro, drive: UIDrive, displaySynchroPath: Bool) {
        var title: String
        if displaySynchroPath {
            title = "\(drive.name) › \(synchro.localPath.lastPathComponent)"
        } else {
            title = "\(drive.name)"
        }

        popUpButton.addItem(
            withTitle: title,
            image: KDriveResources.kdriveFoldersStacked.image,
            color: drive.nsColor,
            representedObject: synchro
        )
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
        return cell
    }

    func outlineViewSelectionDidChange(_ notification: Notification) {
        guard let selectedItem = outlineView.item(atRow: outlineView.selectedRow) as? SidebarItem,
              let path = selectedItem.tab else { return }
        router.setCurrentTab(path)
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
