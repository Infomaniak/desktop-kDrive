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

struct FileOpeningCancelQuery: Codable, Sendable {
    let jobId: Int64
}

struct FileOpeningDownloadQuery: Codable, Sendable {
    let driveDbId: Int64
    @Base64CodedString var remoteNodeId: String
    /// Empty to download the item into the download folder of the user.
    @Base64CodedString var destinationFolderPath: String
}

/// The reply of a FILE_OPENING_DOWNLOAD request: the id of the started download, to be matched with the
/// FILE_OPENING_DOWNLOAD_STATE signals, and the path into which the item is downloaded.
public struct FileOpeningDownloadResponse: Codable, Sendable {
    public let downloadId: Int64
    public let finalPath: String

    public init(downloadId: Int64, finalPath: String) {
        self.downloadId = downloadId
        self.finalPath = finalPath
    }
}
