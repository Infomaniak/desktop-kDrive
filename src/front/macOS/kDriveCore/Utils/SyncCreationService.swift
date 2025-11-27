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

import Foundation

public enum SyncOrigin: Sendable {
    case storedDrive(Drive)
    case availableDrive(AvailableDrive)

    var drive: any DriveRepresentation {
        switch self {
        case .storedDrive(let drive):
            return drive
        case .availableDrive(let availableDrive):
            return availableDrive
        }
    }
}

public struct SyncRemoteFolder: Sendable {
    public static let kDriveRoot = SyncRemoteFolder(path: "", nodeId: "")

    public let path: String
    public let nodeId: String

    public init(path: String, nodeId: String) {
        self.path = path
        self.nodeId = nodeId
    }
}

public struct NewSyncCandidate {
    public let origin: SyncOrigin
    public let remoteFolder: SyncRemoteFolder
    public let localFolder: String
    public let blackList: [String]

    public init(origin: SyncOrigin, remoteFolder: SyncRemoteFolder, localFolder: String, blackList: [String]) {
        self.origin = origin
        self.remoteFolder = remoteFolder
        self.localFolder = localFolder
        self.blackList = blackList
    }
}

public protocol SyncCreator: Sendable {
    func create(from sync: NewSyncCandidate) async throws -> SyncInfo
    func preferredLocalPath(for syncOrigin: SyncOrigin) async throws -> String
}

public final class SyncCreationService: SyncCreator {
    private let useLightSyncIfPossible: Bool

    init(useLightSyncIfPossible: Bool = true) {
        self.useLightSyncIfPossible = useLightSyncIfPossible
    }

    public func create(from sync: NewSyncCandidate) async throws -> SyncInfo {
        let identifier = getIdentifier(from: sync.origin)

        let useLightSync = shouldUseLightSync(for: sync.origin)
        let metadata = getMetadata(for: sync, useLightSync: useLightSync)

        try createDestinationIfNecessary(at: sync.localFolder)

        let syncInfo = try await SyncJobs().addSync(identifier: identifier, metadata: metadata)
        return syncInfo
    }

    public func preferredLocalPath(for syncOrigin: SyncOrigin) async throws -> String {
        // TODO: The server cannot provide the preferred value yet. We have a dummy implementation for the moment.

        let homeDirectory = FileManager.default.homeDirectoryForCurrentUser
        let uuid = UUID()
        return homeDirectory.appendingPathComponent("kDriveTest/kDrive_\(uuid)").path
    }

    private func getIdentifier(from origin: SyncOrigin) -> NewSyncParentIdentifier {
        switch origin {
        case .storedDrive(let drive):
            return .driveDbId(drive.driveDbId)
        case .availableDrive(let availableDrive):
            return .transitive(
                userDbId: availableDrive.userDbId,
                accountId: availableDrive.accountId,
                driveId: availableDrive.driveId
            )
        }
    }

    private func getMetadata(for sync: NewSyncCandidate, useLightSync: Bool) -> NewSyncMetadata {
        let drive = sync.origin.drive
        let metadata = NewSyncMetadata(
            userDbId: drive.userDbId,
            accountId: drive.accountId,
            driveId: drive.driveId,
            localFolderPath: sync.localFolder,
            serverFolderPath: sync.remoteFolder.path,
            serverFolderNodeId: sync.remoteFolder.nodeId,
            liteSync: useLightSync,
            blackList: sync.blackList,
            whiteList: [] // TODO: Remove this parameter when removed from the server object
        )
        return metadata
    }

    private func shouldUseLightSync(for origin: SyncOrigin) -> Bool {
        guard useLightSyncIfPossible else {
            return false
        }

        // TODO: Check if the drive can use the Light Sync for the given origin
        return true
    }

    private func createDestinationIfNecessary(at path: String) throws {
        guard !FileManager.default.fileExists(atPath: path) else {
            return
        }

        try FileManager.default.createDirectory(at: URL(fileURLWithPath: path), withIntermediateDirectories: true)
    }
}
