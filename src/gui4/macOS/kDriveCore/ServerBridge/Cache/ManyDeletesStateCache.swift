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
import CppInterop
import Foundation

public struct ManyDeletesNotification: Hashable, Sendable, Identifiable {
    public var id: Int32 {
        syncDbId
    }

    public let syncDbId: Int32
    public let notificationType: KDC.TooManyDeletesNotificationType
    public let nbFiles: UInt64

    public init(syncDbId: Int32, notificationType: KDC.TooManyDeletesNotificationType, nbFiles: UInt64) {
        self.syncDbId = syncDbId
        self.notificationType = notificationType
        self.nbFiles = nbFiles
    }
}

public typealias ManyDeletesPublisher = AnyPublisher<ManyDeletesNotification, Never>

public protocol ManyDeletesCacheObservable: Sendable {
    var manyDeletesPublisher: ManyDeletesPublisher { get }
}

public protocol ManyDeletesCache {
    func notifyManyDeletes(_ notification: ManyDeletesNotification) async
}

public actor ManyDeletesStateCache: ManyDeletesCache, ManyDeletesCacheObservable {
    private nonisolated let manyDeletesSubject = PassthroughSubject<ManyDeletesNotification, Never>()

    public nonisolated var manyDeletesPublisher: ManyDeletesPublisher {
        manyDeletesSubject
            .subscribe(on: DispatchQueue.global(qos: .userInitiated))
            .eraseToAnyPublisher()
    }

    public init() {}

    public func notifyManyDeletes(_ notification: ManyDeletesNotification) {
        manyDeletesSubject.send(notification)
    }
}
