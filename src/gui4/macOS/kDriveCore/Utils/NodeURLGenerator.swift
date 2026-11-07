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

import Foundation

public protocol NodeURLGenerator: Sendable {
    func localURL(for nodePath: String, synchroPath: URL) -> URL
    func remoteURL(for nodeId: String, driveId: Int) -> URL
    func shareURL(for nodeId: String, driveDbId: Int) async throws -> URL
    func redirectURL(forDriveId driveId: Int, fileId: String) -> URL?
    func shopURL(forDriveId driveId: Int) -> URL
}

public enum NodePathResolver {
    /// Resolves a node path against the sync root folder, without ever involving the process working directory.
    /// `URL(fileURLWithPath:)` applied to a relative path silently resolves it against the CWD, and
    /// `URL(fileURLWithPath:relativeTo:)` gives directory or file merge semantics depending on whether the base
    /// folder exists on disk, so neither is suitable for building the local location of a synced item.
    public static func localURL(for nodePath: String, synchroPath: URL) -> URL {
        guard !nodePath.isEmpty else { return synchroPath }
        // Absolute node paths are tolerated and used as is.
        guard !nodePath.hasPrefix("/") else { return URL(fileURLWithPath: nodePath) }

        return synchroPath.appendingPathComponent(nodePath)
    }
}

public struct DriveNodeURLGenerator: NodeURLGenerator {
    public func localURL(for nodePath: String, synchroPath: URL) -> URL {
        return NodePathResolver.localURL(for: nodePath, synchroPath: synchroPath)
    }

    public func remoteURL(for nodeId: String, driveId: Int) -> URL {
        return URLConstants.kDrive(for: driveId).appendingPathComponent("redirect/\(nodeId)")
    }

    public func shareURL(for nodeId: String, driveDbId: Int) async throws -> URL {
        return try await SyncJobs().getPublicLinkUrl(driveDbId: Int32(driveDbId), nodeId: nodeId)
    }

    public func redirectURL(forDriveId driveId: Int, fileId: String) -> URL? {
        let urlString = "https://kdrive.infomaniak.com/app/drive/\(driveId)/redirect/\(fileId)"
        return URL(string: urlString)
    }

    public func shopURL(forDriveId driveId: Int) -> URL {
        return URL(string: "https://shop.infomaniak.com/order/drive/\(driveId)")!
    }
}
