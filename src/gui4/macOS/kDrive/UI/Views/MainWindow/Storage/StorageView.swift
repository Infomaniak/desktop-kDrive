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
import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import kDriveResources
import OrderedCollections
import SwiftUI

struct StorageItem: Sendable, Identifiable {
    var id: String {
        return "\(title)_\(color)"
    }

    let title: String
    let color: Color
    var usedBytes: Int64?
    let isDefault: Bool
    /// Drive recalled next to the title, if the item refers to a specific drive.
    var drive: UIDrive?
    /// Sync recalled next to the title, if the item refers to a specific sync.
    var synchro: UISynchro?

    init(title: String, color: Color, usedBytes: Int64?, isDefault: Bool = false, drive: UIDrive? = nil, synchro: UISynchro? = nil) {
        self.title = title
        self.color = color
        self.usedBytes = usedBytes
        self.isDefault = isDefault
        self.drive = drive
        self.synchro = synchro
    }
}

struct DriveStorageInfo {
    var name: String
    var size: Int64?
    var usedSize: Int64?
}

enum VolumeStorageItems {
    case freeSpace
    case usedSpace
    case usedByKDrive
}

enum DriveStorageItems {
    case usedSpace
    case freeSpace
}

struct StorageView: View {
    private static let isRunningInPreviews = ProcessInfo.processInfo.environment["XCODE_RUNNING_FOR_PREVIEWS"] == "1"

    @InjectService private var storageDataProviding: StorageDataProviding
    @InjectService private var cacheObservable: CoherentCacheObservable

    @State private var volumeName = KDriveLocalizable.storageDeviceNameMac
    @State private var volumeStorageItems: OrderedDictionary<VolumeStorageItems, StorageItem> = [
        .usedByKDrive: StorageItem(title: KDriveLocalizable.storageMacUsedByKDrive, color: .blue, usedBytes: nil),
        .usedSpace: StorageItem(title: KDriveLocalizable.storageMacUsedByComputer, color: .purple, usedBytes: nil),
        .freeSpace: StorageItem(title: KDriveLocalizable.storageMacFreeSpace, color: .gray, usedBytes: nil, isDefault: true)
    ]
    @State private var isShowingVolumeNotFound = false

    @State private var driveStorageInfo = DriveStorageInfo(
        name: KDriveLocalizable.labelMySubscription,
        size: nil,
        usedSize: nil
    )

    @ObservedObject var mainViewModel: MainViewModel

    private var storagePublisher: AnyPublisher<IndexedStorageData, Never> {
        storageDataProviding.storageDataPublisher.removeDuplicates().eraseToAnyPublisher()
    }

    private var drivePublisher: AnyPublisher<Drive?, Never> {
        guard let driveDbId = mainViewModel.currentDrive?.dbId else {
            return Empty().eraseToAnyPublisher()
        }
        return cacheObservable.usersPublisher.drivePublisher(driveDbId: Int32(driveDbId))
    }

    private var displayedVolumeStorageItems: [StorageItem] {
        var items = volumeStorageItems
        items[.usedByKDrive]?.synchro = mainViewModel.currentSynchro
        items[.usedByKDrive]?.drive = mainViewModel.currentDrive
        return Array(items.values)
    }

    private var volumeStorageData: StorageSectionView.StorageData {
        guard let usedByKDrive = volumeStorageItems[.usedByKDrive]?.usedBytes,
              let usedByComputer = volumeStorageItems[.usedSpace]?.usedBytes,
              let freeSpace = volumeStorageItems[.freeSpace]?.usedBytes else {
            return .loading
        }

        let usedSpace = usedByKDrive + usedByComputer
        return .data(usedBytes: usedSpace, availableBytes: usedSpace + freeSpace)
    }

    private var driveStorageItems: OrderedDictionary<DriveStorageItems, StorageItem> {
        var freeSpace: Int64?
        if let size = driveStorageInfo.size, let usedSize = driveStorageInfo.usedSize {
            freeSpace = max(size - usedSize, 0)
        }

        return [
            .usedSpace: StorageItem(
                title: KDriveLocalizable.storageOnlineUsed,
                color: .blue,
                usedBytes: driveStorageInfo.usedSize,
                drive: mainViewModel.currentDrive
            ),
            .freeSpace: StorageItem(
                title: KDriveLocalizable.storageOnlineFree,
                color: .gray,
                usedBytes: freeSpace,
                isDefault: true
            )
        ]
    }

    private var driveStorageData: StorageSectionView.StorageData {
        guard let usedSpace = driveStorageItems[.usedSpace]?.usedBytes,
              let freeSpace = driveStorageItems[.freeSpace]?.usedBytes else {
            return .loading
        }
        return .data(usedBytes: usedSpace, availableBytes: usedSpace + freeSpace)
    }

    var body: some View {
        ZStack {
            if isShowingVolumeNotFound {
                IKContentUnavailableView(
                    image: KDriveResources.volumeStrokeDots.swiftUIImage,
                    imageMaxWidth: 100,
                    title: KDriveLocalizable.storageMissingDiskMacOSTitle,
                    subtitle: KDriveLocalizable.storageMissingDiskMacOSDescription,
                    action: .init(title: KDriveLocalizable.buttonRetry) {
                        Task {
                            await fetchStorageData()
                        }
                    }
                )
            } else {
                Form {
                    StorageSectionView(
                        title: volumeName,
                        storageData: volumeStorageData,
                        items: displayedVolumeStorageItems,
                        byteCountFormatStyle: .file
                    )

                    Section {
                        InformationBlockContentView(
                            title: KDriveLocalizable.storageSyncBlockTitle,
                            subtitle: KDriveLocalizable.storageSyncBlockMacDescription,
                            button: InformationBlockButton(title: KDriveLocalizable.buttonManage, action: didTapManage)
                        )
                    }

                    StorageSectionView(
                        title: driveStorageInfo.name,
                        storageData: driveStorageData,
                        items: Array(driveStorageItems.values),
                        byteCountFormatStyle: .memory
                    )
                }
                .groupedFormatStyle()
            }
        }
        .onReceive(storagePublisher.receive(on: RunLoop.main), perform: handleUpdatedStorageData)
        .onReceive(drivePublisher.receive(on: RunLoop.main), perform: handleUpdatedDrive)
        .onAppear {
            Task {
                await getCachedStorageData()
            }
        }
        .task(id: mainViewModel.currentSynchro?.dbId) {
            await fetchStorageData()
        }
        .task {
            // Ask the server to reload the drive info from the API, the updated quota is then received through `drivePublisher`.
            guard !Self.isRunningInPreviews else { return }
            try? await UtilityJobs().activateLoadInfo()
        }
    }

    private func didTapManage() {
        (NSApp.delegate as? AppDelegate)?.openPreferencesWindow()

        @InjectService var router: PreferencesViewRouter
        router.setCurrentTab(.accounts)
        if let currentDrive = mainViewModel.currentDrive, let userDbId = mainViewModel.currentUser?.dbId {
            router.append(.syncedKDrive(currentDrive, userDbId: userDbId))
        }
    }

    private func isStandaloneDrivePack(_ packName: String) -> Bool {
        let prefixes = ["solo", "team", "pro", "free"]
        return prefixes.contains { packName.hasPrefix($0) }
    }

    private func fetchStorageData() async {
        guard let synchroDbId = mainViewModel.currentSynchro?.dbId else {
            return
        }
        try? await storageDataProviding.fetchStorageData(forSynchroDbId: Int32(synchroDbId))
    }

    private func handleUpdatedDrive(_ drive: Drive?) {
        // In previews, drive values are pinned by the preview initializer; skip the real ones.
        guard !Self.isRunningInPreviews, let drive else {
            return
        }

        var name = KDriveLocalizable.labelMySubscription
        if isStandaloneDrivePack(drive.packInfo.name) {
            name.append(" kDrive ")
        }
        name.append(drive.packInfo.displayName)

        withAnimation {
            driveStorageInfo = DriveStorageInfo(name: name, size: drive.size, usedSize: drive.usedSize)
        }
    }

    private func getCachedStorageData() async {
        updateVolumeStorage(from: storageDataProviding.storageData)
    }

    private func handleUpdatedStorageData(_ indexedStorageData: IndexedStorageData) {
        withAnimation {
            updateVolumeStorage(from: indexedStorageData)
        }
    }

    private func updateVolumeStorage(from indexedStorageData: IndexedStorageData) {
        guard let synchroDbId = mainViewModel.currentSynchro?.dbId,
              let storageDataResult = indexedStorageData[Int32(synchroDbId)] else {
            return
        }

        switch storageDataResult {
        case .success(let storageData):
            isShowingVolumeNotFound = false

            volumeName = storageData.name

            volumeStorageItems[.usedByKDrive]?.usedBytes = storageData.usedByKDrive
            volumeStorageItems[.usedSpace]?.usedBytes = storageData.usedSpace
            volumeStorageItems[.freeSpace]?.usedBytes = storageData.freeSpace
        case .failure(let error):
            guard error == .cannotGetVolumeInfo else {
                return
            }
            isShowingVolumeNotFound = true
        }
    }
}

#if DEBUG
extension StorageView {
    /// Preview-only initializer that pins the subscription storage values, so previews don't depend on the real drive.
    init(mainViewModel: MainViewModel, previewDriveSize: Int64, previewDriveUsedSize: Int64) {
        self.init(mainViewModel: mainViewModel)
        _driveStorageInfo = State(initialValue: DriveStorageInfo(
            name: KDriveLocalizable.labelMySubscription,
            size: previewDriveSize,
            usedSize: previewDriveUsedSize
        ))
    }
}

#Preview {
    StorageView(
        mainViewModel: MainViewModel(),
        previewDriveSize: 3_000_000_000_000,
        previewDriveUsedSize: 123_456_789_000
    )
}
#endif
