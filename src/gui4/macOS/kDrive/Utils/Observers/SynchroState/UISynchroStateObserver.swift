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
import OrderedCollections
import SwiftUI

public struct UISynchroState: Sendable, Equatable {
    public let errorCount: Int
    public let status: UISynchroStatus

    public init(errorCount: Int, status: UISynchroStatus) {
        self.errorCount = errorCount
        self.status = status
    }

    public init(fromSynchro synchro: Synchro?) {
        self.init(
            errorCount: synchro?.errors.count ?? 0,
            status: UISynchroStatus(syncStatus: synchro?.progress?.syncStatus ?? .Idle) ?? .idle
        )
    }
}

public protocol UISynchroStateObserving: Sendable {
    var synchroState: UISynchroState { get }
    var synchroStatePublisher: AnyPublisher<UISynchroState, Never> { get }

    func observeSynchro(_ synchroDbId: UISynchro.ID)
}

public final class UISynchroStateObserver: UISynchroStateObserving {
    @MainActor public private(set) var synchroState = UISynchroState(errorCount: 0, status: .idle) {
        didSet {
            synchroStateSubject.send(synchroState)
        }
    }

    @MainActor private let synchroStateSubject = PassthroughSubject<UISynchroState, Never>()
    @MainActor public var synchroStatePublisher: AnyPublisher<UISynchroState, Never> {
        synchroStateSubject.eraseToAnyPublisher()
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

        synchroState = UISynchroState(errorCount: 0, status: .idle)

        Task {
            @InjectService var cache: CoherentCache
            let synchro = await cache.getSynchro(synchroDbId: Int32(synchroDbId))
            synchroState = UISynchroState(fromSynchro: synchro)

            @InjectService var cacheObservable: CoherentCacheObservable
            cancellable = cacheObservable.usersPublisher.synchroPublisher(dbId: Int32(synchroDbId))
                .throttle(for: 0.5, scheduler: RunLoop.main, latest: true)
                .map { UISynchroState(fromSynchro: $0) }
                .removeDuplicates()
                .receive(on: RunLoop.main)
                .sink { [weak self] output in
                    self?.synchroState = output
                }
        }
    }
}
