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
import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import OrderedCollections
import SwiftUI

public struct UISynchroState: Sendable, Equatable {
    public let syncDbId: Int32?
    public let isStatusKnown: Bool
    public let errorCount: Int
    public let status: UISynchroStatus

    public init(errorCount: Int, status: UISynchroStatus, syncDbId: Int32? = nil, isStatusKnown: Bool = false) {
        self.syncDbId = syncDbId
        self.isStatusKnown = isStatusKnown
        self.errorCount = errorCount
        self.status = status
    }

    public init(fromSynchro synchro: Synchro?, syncDbId: Int32? = nil) {
        let progress: SynchroProgressInfo? = synchro?.progress
        let syncStatus: KDC.SyncStatus? = progress?.syncStatus
        let status = syncStatus.flatMap { UISynchroStatus(syncStatus: $0) }
        self.init(
            errorCount: synchro?.errors.count ?? 0,
            status: status ?? .idle,
            syncDbId: synchro?.dbId ?? syncDbId,
            isStatusKnown: status != nil
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
    @MainActor private var observationTask: Task<Void, Never>?
    @MainActor private var observationGeneration = UUID()

    public init() {}

    deinit {
        observationTask?.cancel()
    }

    @MainActor
    public func observeSynchro(_ synchroDbId: UISynchro.ID) {
        cancellable?.cancel()
        observationTask?.cancel()
        let generation = UUID()
        observationGeneration = generation
        let syncDbId = Int32(synchroDbId)

        synchroState = UISynchroState(errorCount: 0, status: .idle, syncDbId: syncDbId)

        observationTask = Task { [weak self] in
            @InjectService var cache: CoherentCache
            let synchro = await cache.getSynchro(synchroDbId: syncDbId)
            guard !Task.isCancelled, let self, observationGeneration == generation else { return }
            synchroState = UISynchroState(fromSynchro: synchro, syncDbId: syncDbId)

            @InjectService var cacheObservable: CoherentCacheObservable
            cancellable = cacheObservable.usersPublisher.synchroPublisher(dbId: syncDbId)
                .throttle(for: 0.5, scheduler: RunLoop.main, latest: true)
                .map { UISynchroState(fromSynchro: $0, syncDbId: syncDbId) }
                .removeDuplicates()
                .receive(on: RunLoop.main)
                .sink { [weak self] output in
                    guard let self, observationGeneration == generation else { return }
                    synchroState = output
                }
        }
    }
}
