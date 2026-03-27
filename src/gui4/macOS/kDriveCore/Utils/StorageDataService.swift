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
import InfomaniakDI

public struct VolumeStorageData: Sendable, Equatable {
    public let name: String

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
        let (volumeName, volumeURL) = try await getVolumeInfo(synchroDbId: synchroDbId)

        async let volumeStorage = fetchVolumeStorage(volumeURL: volumeURL)
        async let kDriveStorage = fetchSynchroStorage(synchroDbId: synchroDbId)

        let resolvedVolumeStorage = try await volumeStorage

        if storageData[synchroDbId] == nil {
            storageData[synchroDbId] = VolumeStorageData(
                name: volumeName,
                freeSpace: resolvedVolumeStorage.availableStorage,
                usedSpace: resolvedVolumeStorage.usedStorage,
                usedByKDrive: nil
            )
            storageDataSubject.send(storageData)
        }

        let resolvedKDriveStorage = try await kDriveStorage
        let usedByComputer = resolvedVolumeStorage.usedStorage - resolvedKDriveStorage

        storageData[synchroDbId] = VolumeStorageData(
            name: volumeName,
            freeSpace: resolvedVolumeStorage.availableStorage,
            usedSpace: usedByComputer,
            usedByKDrive: resolvedKDriveStorage
        )
        storageDataSubject.send(storageData)
    }

    private func fetchVolumeStorage(volumeURL: URL) async throws -> (usedStorage: Int64, availableStorage: Int64) {
        let values = try volumeURL.resourceValues(forKeys: [
            .volumeTotalCapacityKey,
            .volumeAvailableCapacityForImportantUsageKey
        ])

        let totalStorage = Int64(values.volumeTotalCapacity ?? 0)
        let availableStorage = Int64(values.volumeAvailableCapacityForImportantUsage ?? 0)
        let usedStorage = max(0, totalStorage - availableStorage)

        return (usedStorage, availableStorage)
    }

    private func fetchSynchroStorage(synchroDbId: Int32) async throws -> Int64 {
        let filesSize = try await SyncJobs().getOfflineFilesSize(syncDbId: synchroDbId)
        return Int64(filesSize)
    }

    private func getVolumeInfo(synchroDbId: Int32) async throws -> (name: String, url: URL) {
        @InjectService var cache: CoherentCache
        let synchro = await cache.getSynchro(synchroDbId: synchroDbId)

        guard let path = synchro?.localPath else {
            throw CocoaError(.fileReadUnknown)
        }

        let url = URL(fileURLWithPath: path)
        let resourceValues = try url.resourceValues(forKeys: [.volumeURLKey, .volumeNameKey])

        guard let volumeURL = resourceValues.volume else {
            throw CocoaError(.fileReadUnknown)
        }

        guard let volumeName = resourceValues.volumeName else {
            throw CocoaError(.fileReadUnknown)
        }

        return (volumeName, volumeURL)
    }
}
