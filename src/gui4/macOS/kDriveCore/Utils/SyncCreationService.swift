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

    public var drive: any DriveRepresentation {
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
    public let localFolder: URL?
    public let blackList: [String]

    public init(origin: SyncOrigin, remoteFolder: SyncRemoteFolder, localFolder: URL?, blackList: [String]) {
        self.origin = origin
        self.remoteFolder = remoteFolder
        self.localFolder = localFolder
        self.blackList = blackList
    }
}

public protocol SyncCreator: Sendable {
    func create(from sync: NewSyncCandidate) async throws -> SyncInfo
    func preferredLocalPath(for driveName: String) async throws -> URL
}

public final class SyncCreationService: SyncCreator {
    private let useLightSyncIfPossible: Bool

    public init(useLightSyncIfPossible: Bool = true) {
        self.useLightSyncIfPossible = useLightSyncIfPossible
    }

    public func create(from sync: NewSyncCandidate) async throws -> SyncInfo {
        let identifier = getIdentifier(from: sync.origin)

        let localFolderURL = try await getLocalFolderURL(for: sync)

        let useLightSync = try await shouldUseLightSync(at: localFolderURL)
        let metadata = getMetadata(for: sync, useLightSync: useLightSync, localFolderURL: localFolderURL)

        try createDestinationIfNecessary(at: localFolderURL)

        let syncInfo = try await SyncJobs().addSync(identifier: identifier, metadata: metadata)
        return syncInfo
    }

    public func preferredLocalPath(for driveName: String) async throws -> URL {
        let defaultPath = computeDefaultFolderPath(driveName: driveName)
        let path = try await UtilityJobs().getGoodPathForNewSynchro(basePath: defaultPath.path)
        return URL(fileURLWithPath: path)
    }

    private func computeDefaultFolderPath(driveName: String) -> URL {
        let homeDirectory = FileManager.default.homeDirectoryForCurrentUser

        var name = driveName
        if driveName.lowercased().hasPrefix("kdrive") {
            name = driveName.replacingOccurrences(of: "kdrive", with: "", options: .caseInsensitive)
        }

        let folderName = "kDrive \(name)".trimmingCharacters(in: .whitespacesAndNewlines)

        return homeDirectory.appendingPathComponent(folderName)
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

    private func getMetadata(for sync: NewSyncCandidate, useLightSync: Bool, localFolderURL: URL) -> NewSyncMetadata {
        let drive = sync.origin.drive
        return NewSyncMetadata(
            userDbId: drive.userDbId,
            accountId: drive.accountId,
            driveId: drive.driveId,
            localFolderPath: localFolderURL.path,
            serverFolderPath: sync.remoteFolder.path,
            serverFolderNodeId: sync.remoteFolder.nodeId,
            liteSync: useLightSync,
            blackList: sync.blackList
        )
    }

    private func shouldUseLightSync(at url: URL) async throws -> Bool {
        guard useLightSyncIfPossible else {
            return false
        }

        let bestMode = try await UtilityJobs().getBestVirtualFileSystemMode(path: url.path)
        return bestMode == .Mac
    }

    private func getLocalFolderURL(for sync: NewSyncCandidate) async throws -> URL {
        if let providedPath = sync.localFolder {
            return providedPath
        } else {
            return try await preferredLocalPath(for: sync.origin.drive.name)
        }
    }

    private func createDestinationIfNecessary(at url: URL) throws {
        guard !FileManager.default.fileExists(atPath: url.path) else {
            return
        }

        try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
    }
}
