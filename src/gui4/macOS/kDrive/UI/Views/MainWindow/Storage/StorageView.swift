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

    init(title: String, color: Color, usedBytes: Int64?, isDefault: Bool = false) {
        self.title = title
        self.color = color
        self.usedBytes = usedBytes
        self.isDefault = isDefault
    }
}

enum VolumeStorageItems {
    case freeSpace
    case usedSpace
    case usedByKDrive
}

struct StorageView: View {
    static let sizeFormatter = ByteCountFormatStyle.byteCount(style: .file)

    @InjectService private var storageDataProviding: StorageDataProviding

    @State private var volumeName = KDriveLocalizable.storageDeviceNameMac
    @State private var volumeStorageItems: OrderedDictionary<VolumeStorageItems, StorageItem> = [
        .usedByKDrive: StorageItem(title: KDriveLocalizable.storageMacUsedByKDrive, color: .blue, usedBytes: nil),
        .usedSpace: StorageItem(title: KDriveLocalizable.storageMacUsedByComputer, color: .purple, usedBytes: nil),
        .freeSpace: StorageItem(title: KDriveLocalizable.storageMacFreeSpace, color: .gray, usedBytes: nil, isDefault: true)
    ]

    @State private var isShowingVolumeNotFound = false

    @ObservedObject var mainViewModel: MainViewModel

    private var macStorageData: StorageSectionView.StorageData {
        guard let usedByKDrive = volumeStorageItems[.usedByKDrive]?.usedBytes,
              let usedByComputer = volumeStorageItems[.usedSpace]?.usedBytes,
              let freeSpace = volumeStorageItems[.freeSpace]?.usedBytes else {
            return .loading
        }

        let usedSpace = usedByKDrive + usedByComputer
        return .data(usedBytes: usedSpace, availableBytes: usedSpace + freeSpace)
    }

    var body: some View {
        ZStack {
            if isShowingVolumeNotFound {
                IKContentUnavailableView(
                    image: KDriveResources.volumeStrokeDots.swiftUIImage,
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
                    StorageSectionView(title: volumeName, storageData: macStorageData, items: Array(volumeStorageItems.values))

                    Section {
                        InformationBlockContentView(
                            title: KDriveLocalizable.storageSyncBlockTitle,
                            subtitle: KDriveLocalizable.storageSyncBlockMacDescription,
                            button: InformationBlockButton(title: KDriveLocalizable.buttonManage, action: didTapFreeUpSpace)
                        )
                    }
                }
                .groupedFormatStyle()
            }
        }
        .onReceive(
            storageDataProviding.storageDataPublisher.removeDuplicates(),
            perform: handleUpdatedStorageData
        )
        .onAppear {
            getCachedStorageData()
        }
        .task(id: mainViewModel.currentSynchro?.dbId) {
            await fetchStorageData()
        }
    }

    private func didTapFreeUpSpace() {
        // TODO: Redirect to Settings/Synchro
    }

    private func fetchStorageData() async {
        guard let synchroDbId = mainViewModel.currentSynchro?.dbId else {
            return
        }

        try? await storageDataProviding.fetchStorageData(forSynchroDbId: Int32(synchroDbId))
    }

    private func getCachedStorageData() {
        updateMacStorage(from: storageDataProviding.storageData)
    }

    private func handleUpdatedStorageData(_ indexedStorageData: IndexedStorageData) {
        withAnimation {
            updateMacStorage(from: indexedStorageData)
        }
    }

    private func updateMacStorage(from indexedStorageData: IndexedStorageData) {
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

#Preview {
    StorageView(mainViewModel: MainViewModel())
}
