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

public typealias UpdateStatePublisher = AnyPublisher<KDC.UpdateState, Never>

public protocol UpdaterCacheObservable: Sendable {
    var updateStatePublisher: UpdateStatePublisher { get }
}

public protocol UpdaterCache {
    func setUpdateState(_ state: KDC.UpdateState) async
    func getUpdateState() async -> KDC.UpdateState?
}

public actor UpdaterStateCache: UpdaterCache, UpdaterCacheObservable {
    private var updateState: KDC.UpdateState?

    private nonisolated let updateStateSubject = PassthroughSubject<KDC.UpdateState, Never>()

    public nonisolated var updateStatePublisher: UpdateStatePublisher {
        updateStateSubject
            .subscribe(on: DispatchQueue.global(qos: .userInitiated))
            .eraseToAnyPublisher()
    }

    public init() {}

    public func setUpdateState(_ state: KDC.UpdateState) {
        updateState = state
        notifyUpdateStateChange(state)
    }

    public func getUpdateState() -> KDC.UpdateState? {
        updateState
    }

    private func notifyUpdateStateChange(_ state: KDC.UpdateState) {
        updateStateSubject.send(state)
    }
}
