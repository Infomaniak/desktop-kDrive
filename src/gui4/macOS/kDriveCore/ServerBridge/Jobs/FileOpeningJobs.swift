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

import CppInterop
import Foundation
import InfomaniakDI

public struct FileOpeningJobs: Sendable {
    @LazyInjectService private var queryFetcher: XPCQueryFetcherProtocol

    public init() {}

    /// Cancels the file opening job identified by `jobId`, upon user request in the progress window.
    public func cancelFileOpening(jobId: Int64) async throws {
        let query = FileOpeningCancelQuery(jobId: jobId)
        let request = await RequestMessage<FileOpeningCancelQuery>(
            num: RequestNum.FILE_OPENING_CANCEL,
            body: query
        )

        try await queryFetcher.query(request, responseType: CallbackMessage<EmptyResponse>.self)
    }

    /// Starts the download, into `destinationFolderPath`, or into the download folder of the user when it is empty,
    /// of the item of a failed file opening request. The reply carries the id of the started download, to be matched
    /// with the download state signals, and the path into which the item is downloaded.
    public func downloadFileOpeningItem(driveDbId: Int64, remoteNodeId: String,
                                        destinationFolderPath: String) async throws -> FileOpeningDownloadResponse {
        let query = FileOpeningDownloadQuery(driveDbId: driveDbId, remoteNodeId: remoteNodeId,
                                             destinationFolderPath: destinationFolderPath)
        let request = await RequestMessage<FileOpeningDownloadQuery>(
            num: RequestNum.FILE_OPENING_DOWNLOAD,
            body: query
        )

        return try await queryFetcher.query(request, responseType: CallbackMessage<FileOpeningDownloadResponse>.self).body
    }
}
