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
import Foundation

public struct VolumeStorageData: Sendable, Equatable {
    public let freeSpace: Int64
    public let usedSpace: Int64
    public let usedByKDrive: Int64?
}

public typealias IndexedStorageData = [Int32: VolumeStorageData]

public protocol StorageDataProviding: Sendable {
    var storageData: IndexedStorageData { get }
    var storageDataPublisher: AnyPublisher<IndexedStorageData, Never> { get }

    func fetchStorageData(forSynchroDbId synchroDbId: Int32) async throws
}

public final class StorageDataService: StorageDataProviding {
    @MainActor
    public private(set) var storageData: IndexedStorageData = [:]

    @MainActor
    public var storageDataPublisher: AnyPublisher<IndexedStorageData, Never> {
        return storageDataSubject
            .subscribe(on: RunLoop.main)
            .eraseToAnyPublisher()
    }

    @MainActor
    private let storageDataSubject = PassthroughSubject<IndexedStorageData, Never>()

    public init() {}

    @MainActor
    public func fetchStorageData(forSynchroDbId synchroDbId: Int32) async throws {
        async let volumeStorage = fetchVolumeStorage(synchroDbId: synchroDbId)
        async let kDriveStorage = fetchSynchroStorage(synchroDbId: synchroDbId)

        let resolvedVolumeStorage = try await volumeStorage

        if storageData[synchroDbId] == nil {
            storageData[synchroDbId] = VolumeStorageData(
                freeSpace: resolvedVolumeStorage.availableStorage,
                usedSpace: resolvedVolumeStorage.usedStorage,
                usedByKDrive: nil
            )
            storageDataSubject.send(storageData)
        }

        let resolvedKDriveStorage = try await kDriveStorage
        let usedByComputer = resolvedVolumeStorage.usedStorage - resolvedKDriveStorage

        storageData[synchroDbId] = VolumeStorageData(
            freeSpace: resolvedVolumeStorage.availableStorage,
            usedSpace: usedByComputer,
            usedByKDrive: resolvedKDriveStorage
        )
        storageDataSubject.send(storageData)
    }

    private func fetchDeviceStorage() async throws -> (usedStorage: Int64, availableStorage: Int64) {
        let rootURL = URL(fileURLWithPath: "/")
        let values = try rootURL.resourceValues(forKeys: [.volumeTotalCapacityKey, .volumeAvailableCapacityForImportantUsageKey])

        let totalStorage = Int64(values.volumeTotalCapacity ?? 0)
        let availableStorage = Int64(values.volumeAvailableCapacityForImportantUsage ?? 0)
        let usedStorage = max(0, totalStorage - availableStorage)

        return (usedStorage, availableStorage)
    }

    private func fetchSynchroStorage(synchroDbId: Int32) async throws -> Int64 {
        let filesSize = try await SyncJobs().getOfflineFilesSize(syncDbId: synchroDbId)
        return Int64(filesSize)
    }
}
