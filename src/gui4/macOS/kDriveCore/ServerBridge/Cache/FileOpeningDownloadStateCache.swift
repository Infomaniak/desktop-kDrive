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

import Combine
import Foundation

/// State of the download of an item triggered from the failure window of a file opening request.
public struct FileOpeningDownloadStateUpdate: Hashable, Sendable {
    public let downloadId: Int64
    public let state: FileOpeningDownloadState
    public let percent: Int
    public let receivedBytes: Int64
    public let totalBytes: Int64
    public let finalPath: String

    public init(downloadId: Int64, state: FileOpeningDownloadState, percent: Int, receivedBytes: Int64,
                totalBytes: Int64, finalPath: String) {
        self.downloadId = downloadId
        self.state = state
        self.percent = percent
        self.receivedBytes = receivedBytes
        self.totalBytes = totalBytes
        self.finalPath = finalPath
    }
}

public typealias FileOpeningDownloadStatePublisher = AnyPublisher<FileOpeningDownloadStateUpdate, Never>

public protocol FileOpeningDownloadStateCacheObservable: Sendable {
    var fileOpeningDownloadStatePublisher: FileOpeningDownloadStatePublisher { get }
}

public protocol FileOpeningDownloadStateCaching: Sendable {
    func update(_ update: FileOpeningDownloadStateUpdate) async
}

/// Publishes the state of the downloads triggered from the failure windows of the file opening flow.
public actor FileOpeningDownloadStateCache: FileOpeningDownloadStateCaching, FileOpeningDownloadStateCacheObservable {
    private nonisolated let fileOpeningDownloadStateSubject = PassthroughSubject<FileOpeningDownloadStateUpdate, Never>()

    public nonisolated var fileOpeningDownloadStatePublisher: FileOpeningDownloadStatePublisher {
        fileOpeningDownloadStateSubject
            .subscribe(on: DispatchQueue.global(qos: .userInitiated))
            .eraseToAnyPublisher()
    }

    public init() {}

    public func update(_ update: FileOpeningDownloadStateUpdate) {
        fileOpeningDownloadStateSubject.send(update)
    }
}
