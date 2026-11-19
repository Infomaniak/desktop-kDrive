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

/// Pure-Swift mirror of `KDC.FileOpeningDownloadState` for types crossing the framework boundary into the UI layer.
public enum FileOpeningDownloadState: Int, Codable, Sendable, Hashable {
    case downloading = 0
    case done = 1
    case failed = 2
}

struct FileOpeningDownloadStateSignal: Codable, Sendable {
    let downloadId: Int64
    let state: FileOpeningDownloadState
    let percent: Int
    let receivedBytes: Int64
    let totalBytes: Int64
    @Base64CodedString var finalPath: String
}
