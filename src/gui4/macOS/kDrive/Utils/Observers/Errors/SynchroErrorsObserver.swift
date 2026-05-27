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
import SwiftUI
import OrderedCollections

public protocol SynchroErrorsObserving: Sendable {
    var synchroErrors: [UISynchroErrorCategory: [SynchroError]] { get }
    var synchroErrorsPublisher: AnyPublisher<[UISynchroErrorCategory: [SynchroError]], Never> { get }

    func observeSynchro(_ synchroDbId: UISynchro.ID)
}

public final class SynchroErrorsObserver: SynchroErrorsObserving {
    @MainActor public private(set) var synchroErrors: [UISynchroErrorCategory: [SynchroError]] = [:] {
        didSet {
            synchroErrorsSubject.send(synchroErrors)
        }
    }

    @MainActor private let synchroErrorsSubject = PassthroughSubject<[UISynchroErrorCategory: [SynchroError]], Never>()
    @MainActor public var synchroErrorsPublisher: AnyPublisher<[UISynchroErrorCategory: [SynchroError]], Never> {
        synchroErrorsSubject.eraseToAnyPublisher()
    }

    @MainActor private var cancellable: AnyCancellable?

    private let categorizer = UISynchroErrorCategorizer()

    public init() {}

    deinit {
        Task { @MainActor [weak self] in
            self?.cancellable?.cancel()
        }
    }

    @MainActor
    public func observeSynchro(_ synchroDbId: UISynchro.ID) {
        cancellable?.cancel()

        synchroErrors = [:]

        Task {
            @InjectService var cache: CoherentCache
            let synchro = await cache.getSynchro(synchroDbId: Int32(synchroDbId))
            let errors = synchro?.errors ?? [:]

            synchroErrors = categorizeErrors(Array(errors.values))

            @InjectService var cacheObservable: CoherentCacheObservable
            cancellable = cacheObservable.usersPublisher.synchroPublisher(dbId: Int32(synchroDbId))
                .throttle(for: 1, scheduler: RunLoop.main, latest: true)
                .compactMap { $0?.errors.values }
                .removeDuplicates()
                .map { self.categorizeErrors(Array($0)) }
                .receive(on: RunLoop.main)
                .sink { [weak self] categorizedErrors in
                    self?.synchroErrors = categorizedErrors
                }
        }
    }

    private func categorizeErrors(_ errors: [ErrorInfo]) -> [UISynchroErrorCategory: [SynchroError]] {
        var categorizedErrors = [UISynchroErrorCategory: [SynchroError]]()
        for error in errors {
            let synchroError = SynchroError(errorInfo: error)
            let category = categorizer.categorize(error: error, kind: synchroError.kind)

            categorizedErrors[category, default: []].append(synchroError)
        }


        return categorizedErrors
    }
}
