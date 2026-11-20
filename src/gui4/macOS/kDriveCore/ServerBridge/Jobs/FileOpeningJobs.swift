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
}
