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

    init(
        title: String,
        color: Color,
        usedBytes: Int64?,
        isDefault: Bool = false,
        drive: UIDrive? = nil,
        synchro: UISynchro? = nil
    ) {
        self.title = title
        self.color = color
        self.usedBytes = usedBytes
        self.isDefault = isDefault
        self.drive = drive
        self.synchro = synchro
    }
}

struct DriveStorageInfo {
    /// Database id of the drive these values belong to, `nil` when they don't come from a drive.
    let driveDbId: Int?
    let name: String
    let size: Int64
    let usedSize: Int64

    var freeSpace: Int64 {
        return max(size - usedSize, 0)
    }
}

struct StorageView: View {
    @InjectService private var storageDataProviding: StorageDataProviding
    @InjectService private var cacheObservable: CoherentCacheObservable
    @InjectService private var coherentCache: CoherentCache

    /// Storage results of every sync, the displayed values are derived from the selected sync's one.
    @State private var indexedStorageData: IndexedStorageData = [:]

    /// Storage info of the last drive received, the displayed values are derived from it if it is the selected drive.
    @State private var driveStorageInfo: DriveStorageInfo?

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

    // MARK: - Volume section

    private var currentVolumeStorageResult: Result<VolumeStorageData, StorageDataError>? {
        guard let synchroDbId = mainViewModel.currentSynchro?.dbId else {
            return nil
        }
        return indexedStorageData[Int32(synchroDbId)]
    }

    /// Measurements of the selected sync's volume, `nil` while they are loading or couldn't be fetched.
    private var currentVolumeStorage: VolumeStorageData? {
        guard case .success(let volumeStorage) = currentVolumeStorageResult else {
            return nil
        }
        return volumeStorage
    }

    private var isShowingVolumeNotFound: Bool {
        return currentVolumeStorageResult == .failure(.cannotGetVolumeInfo)
    }

    private var volumeName: String {
        return currentVolumeStorage?.name ?? KDriveLocalizable.storageDeviceNameMac
    }

    private var volumeStorageItems: [StorageItem] {
        let volumeStorage = currentVolumeStorage
        return [
            StorageItem(
                title: KDriveLocalizable.storageMacUsedByKDrive,
                color: .blue,
                usedBytes: volumeStorage?.usedByKDrive,
                drive: mainViewModel.currentDrive,
                synchro: mainViewModel.currentSynchro
            ),
            StorageItem(title: KDriveLocalizable.storageMacUsedByComputer, color: .purple, usedBytes: volumeStorage?.usedSpace),
            StorageItem(
                title: KDriveLocalizable.storageMacFreeSpace,
                color: .gray,
                usedBytes: volumeStorage?.freeSpace,
                isDefault: true
            )
        ]
    }

    private var volumeStorageData: StorageSectionView.StorageData {
        guard let volumeStorage = currentVolumeStorage, let usedByKDrive = volumeStorage.usedByKDrive else {
            return .loading
        }

        let usedSpace = usedByKDrive + volumeStorage.usedSpace
        return .data(usedBytes: usedSpace, availableBytes: usedSpace + volumeStorage.freeSpace)
    }

    // MARK: - Subscription section

    /// Storage info of the selected drive, `nil` while it is loading or the info received belongs to another drive.
    private var currentDriveStorage: DriveStorageInfo? {
        guard let driveStorageInfo, driveStorageInfo.driveDbId == mainViewModel.currentDrive?.dbId else {
            return nil
        }
        return driveStorageInfo
    }

    private var driveName: String {
        return currentDriveStorage?.name ?? KDriveLocalizable.labelMySubscription
    }

    private var driveStorageItems: [StorageItem] {
        let driveStorage = currentDriveStorage
        return [
            StorageItem(
                title: KDriveLocalizable.storageOnlineUsed,
                color: .blue,
                usedBytes: driveStorage?.usedSize,
                drive: mainViewModel.currentDrive
            ),
            StorageItem(
                title: KDriveLocalizable.storageOnlineFree,
                color: .gray,
                usedBytes: driveStorage?.freeSpace,
                isDefault: true
            )
        ]
    }

    private var driveStorageData: StorageSectionView.StorageData {
        guard let driveStorage = currentDriveStorage else {
            return .loading
        }

        return .data(usedBytes: driveStorage.usedSize, availableBytes: driveStorage.usedSize + driveStorage.freeSpace)
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
                        items: volumeStorageItems,
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
                        title: driveName,
                        storageData: driveStorageData,
                        items: driveStorageItems,
                        byteCountFormatStyle: .memory
                    )
                }
                .groupedFormatStyle()
            }
        }
        .onReceive(storagePublisher.receive(on: RunLoop.main), perform: handleUpdatedStorageData)
        .onAppear(perform: getCachedStorageData)
        .task(id: mainViewModel.currentDrive?.dbId) {
            await observeCurrentDrive()
        }
        .task(id: mainViewModel.currentSynchro?.dbId) {
            await fetchStorageData()
        }
        .task {
            // Ask the server to reload the drive info from the API, the updated quota is then received through `drivePublisher`.
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

    /// Restarted each time the selected drive changes, as `onReceive` would keep its first subscription.
    private func observeCurrentDrive() async {
        guard let driveDbId = mainViewModel.currentDrive?.dbId else {
            return
        }

        // `drivePublisher` only emits on cache updates, so start from the drive already in the cache.
        await handleUpdatedDrive(coherentCache.getDrive(driveDbId: Int32(driveDbId)))

        for await drive in drivePublisher.values {
            handleUpdatedDrive(drive)
        }
    }

    private func handleUpdatedDrive(_ drive: Drive?) {
        // Drives restored from the server database have a size but no used size nor pack, these are only set once the drive
        // info has been loaded from the API. The pack id tells whether the quota comes from the API.
        guard let drive, drive.packInfo.id != 0 else {
            driveStorageInfo = nil
            return
        }

        var name = KDriveLocalizable.labelMySubscription + " "
        if isStandaloneDrivePack(drive.packInfo.name) {
            name.append("kDrive ")
        }
        name.append(drive.packInfo.displayName)

        withAnimation {
            driveStorageInfo = DriveStorageInfo(
                driveDbId: Int(drive.driveDbId),
                name: name,
                size: drive.size,
                usedSize: drive.usedSize
            )
        }
    }

    private func getCachedStorageData() {
        indexedStorageData = storageDataProviding.storageData
    }

    private func handleUpdatedStorageData(_ indexedStorageData: IndexedStorageData) {
        withAnimation {
            self.indexedStorageData = indexedStorageData
        }
    }
}

#if DEBUG
extension StorageView {
    /// Preview-only initializer that pins the subscription storage values, so previews don't depend on the real drive.
    init(mainViewModel: MainViewModel, previewDriveSize: Int64, previewDriveUsedSize: Int64) {
        self.init(mainViewModel: mainViewModel)
        _driveStorageInfo = State(initialValue: DriveStorageInfo(
            driveDbId: mainViewModel.currentDrive?.dbId,
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
