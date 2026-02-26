/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2025 Infomaniak Network SA

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
import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import SwiftUI

public protocol UISynchroNodesObserving: Sendable {
    var synchroNodes: [UISynchroNodeContext] { get }
    var synchroNodesPublisher: AnyPublisher<[UISynchroNodeContext], Never> { get }

    func observeSynchro(_ synchroDbId: UISynchro.ID)
}

public final class UISynchroNodesObserver: UISynchroNodesObserving {
    @MainActor public private(set) var synchroNodes: [UISynchroNodeContext] = [] {
        didSet {
            synchroNodesSubject.send(synchroNodes)
        }
    }

    @MainActor private let synchroNodesSubject = PassthroughSubject<[UISynchroNodeContext], Never>()
    @MainActor public var synchroNodesPublisher: AnyPublisher<[UISynchroNodeContext], Never> {
        synchroNodesSubject.eraseToAnyPublisher()
    }

    @MainActor private var cancellable: AnyCancellable?

    public init() {}

    deinit {
        Task { @MainActor [weak self] in
            self?.cancellable?.cancel()
        }
    }

    @MainActor
    public func observeSynchro(_ synchroDbId: UISynchro.ID) {
        cancellable?.cancel()

        synchroNodes = []

        Task {
            @InjectService var cache: CoherentCache
            let nodeContexts = await cache.getSynchroNodeContexts(Int32(synchroDbId))
            synchroNodes = nodeContexts.map(UISynchroNodeContext.init)

            @InjectService var cacheObservable: CoherentCacheObservable
            cancellable = cacheObservable.usersPublisher.synchroNodesPublisher(for: Int32(synchroDbId))
                .throttle(for: 1, scheduler: RunLoop.main, latest: true)
                .map { $0.map(UISynchroNodeContext.init) }
                .receive(on: RunLoop.main)
                .sink { [weak self] nodeContexts in
                    self?.synchroNodes = nodeContexts
                }
        }
    }
}
