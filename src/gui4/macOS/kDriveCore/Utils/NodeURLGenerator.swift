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
}

public struct DriveNodeURLGenerator: NodeURLGenerator {
    public func localURL(for nodePath: String, synchroPath: URL) -> URL {
        return synchroPath.appendingPathComponent(nodePath)
    }

    public func remoteURL(for nodeId: String, driveId: Int) -> URL {
        return URLConstants.kDrive(for: driveId).appendingPathComponent("redirect/\(nodeId)")
    }

    public func shareURL(for nodeId: String, driveDbId: Int) async throws -> URL {
        return try await SyncJobs().getPublicLinkUrl(driveDbId: Int32(driveDbId), nodeId: nodeId)
    }
}
