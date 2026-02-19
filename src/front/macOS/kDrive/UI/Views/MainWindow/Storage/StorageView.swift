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

enum MacStorageItems {
    case usedByKDrive
    case usedByComputer
    case freeSpace
}

struct StorageView: View {
    static let sizeFormatter = ByteCountFormatStyle.byteCount(style: .file)

    @ObservedObject var mainViewModel: MainViewModel

    @State private var macStorageItems: OrderedDictionary<MacStorageItems, StorageItem> = [
        .usedByKDrive: StorageItem(title: KDriveLocalizable.storageMacUsedByKDrive, color: .blue, usedBytes: nil),
        .usedByComputer: StorageItem(title: KDriveLocalizable.storageMacUsedByComputer, color: .purple, usedBytes: nil),
        .freeSpace: StorageItem(title: KDriveLocalizable.storageMacFreeSpace, color: .gray, usedBytes: nil, isDefault: true)
    ]

    private var deviceName: String {
        return Host().localizedName ?? KDriveLocalizable.storageDeviceNameMac
    }

    private var macStorageData: StorageSectionView.StorageData {
        guard let usedByKDrive = macStorageItems[.usedByKDrive]?.usedBytes,
              let usedByComputer = macStorageItems[.usedByComputer]?.usedBytes,
              let freeSpace = macStorageItems[.freeSpace]?.usedBytes else {
            return .loading
        }

        let usedSpace = usedByKDrive + usedByComputer
        return .data(usedBytes: usedSpace, availableBytes: usedSpace + freeSpace)
    }

    var body: some View {
        Form {
            StorageSectionView(title: deviceName, storageData: macStorageData, items: Array(macStorageItems.values))

            Section {
                InformationBlockContentView(
                    title: KDriveLocalizable.storageSyncBlockTitle,
                    subtitle: KDriveLocalizable.storageSyncBlockMacDescription,
                    button: InformationBlockButton(title: KDriveLocalizable.buttonManage, action: didTapFreeUpSpace)
                )
            }
        }
        .groupedFormatStyle()
        .padding(AppPadding.page)
        .task(id: mainViewModel.currentSynchro?.dbId) {
            try? await computeStorageData()
        }
    }

    private func didTapFreeUpSpace() {
        // TODO: Redirect to Settings/Synchro
    }

    private func computeStorageData() async throws {
        async let (usedByComputer, availableStorage) = fetchAvailableStorage()
        async let kDriveStorage = try fetchSyncStorage()

        macStorageItems[.usedByComputer]?.usedBytes = try await usedByComputer
        macStorageItems[.freeSpace]?.usedBytes = try await availableStorage
        macStorageItems[.usedByKDrive]?.usedBytes = try await kDriveStorage
    }

    private func fetchSyncStorage() async throws -> Int64 {
        guard let synchroDbId = mainViewModel.currentSynchro?.dbId else {
            return 0
        }

        let filesSize = try await SyncJobs().getOfflineFilesSize(syncDbId: Int32(synchroDbId))
        return Int64(filesSize)
    }

    private func fetchAvailableStorage() async throws -> (usedByComputer: Int64, availableStorage: Int64) {
        let rootURL = URL(fileURLWithPath: "/")
        let values = try rootURL.resourceValues(forKeys: [.volumeTotalCapacityKey, .volumeAvailableCapacityForImportantUsageKey])

        let totalStorage = Int64(values.volumeTotalCapacity ?? 0)
        let availableStorage = Int64(values.volumeAvailableCapacityForImportantUsage ?? 0)
        let usedStorage = totalStorage - availableStorage

        return (usedStorage, availableStorage)
    }
}

#Preview {
    StorageView(mainViewModel: MainViewModel())
}
