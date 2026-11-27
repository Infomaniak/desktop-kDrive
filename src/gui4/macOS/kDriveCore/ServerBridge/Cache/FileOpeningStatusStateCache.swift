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

/// Pure-Swift mirror of `KDC.FileOpeningPhase` for types crossing the framework boundary into the UI layer.
public enum FileOpeningStatusPhase: Int, Codable, Sendable, Hashable {
    case finished = 0
    case waitingSync = 1
    case hydrating = 2
}

public struct FileOpeningStatus: Hashable, Sendable {
    public let jobId: Int64
    public let phase: FileOpeningStatusPhase
    public let percent: Int
    public let itemName: String

    public init(jobId: Int64, phase: FileOpeningStatusPhase, percent: Int, itemName: String) {
        self.jobId = jobId
        self.phase = phase
        self.percent = percent
        self.itemName = itemName
    }
}

public typealias FileOpeningStatusPublisher = AnyPublisher<FileOpeningStatus, Never>

public protocol FileOpeningStatusCacheObservable: Sendable {
    var fileOpeningStatusPublisher: FileOpeningStatusPublisher { get }
}

public protocol FileOpeningStatusCache: Sendable {
    func update(_ status: FileOpeningStatus) async
}

public actor FileOpeningStatusStateCache: FileOpeningStatusCache, FileOpeningStatusCacheObservable {
    private nonisolated let fileOpeningStatusSubject = PassthroughSubject<FileOpeningStatus, Never>()

    public nonisolated var fileOpeningStatusPublisher: FileOpeningStatusPublisher {
        fileOpeningStatusSubject
            .subscribe(on: DispatchQueue.global(qos: .userInitiated))
            .eraseToAnyPublisher()
    }

    public init() {}

    public func update(_ status: FileOpeningStatus) {
        fileOpeningStatusSubject.send(status)
    }
}
