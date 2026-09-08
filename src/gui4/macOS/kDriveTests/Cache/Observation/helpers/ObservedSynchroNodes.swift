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
import InfomaniakDI
import kDriveCore
import OrderedCollections

@MainActor
@propertyWrapper
final class ObservedSynchroNodes: ObservableObject {
    @Published private(set) var wrappedValue: [SynchroNodeContext] = []
    private var cancellable: AnyCancellable?

    init(
        synchroDbId: Int32,
        cacheObservation: CoherentCacheObservable? = nil
    ) {
        let cacheObservation =
            cacheObservation ?? InjectService<CoherentCacheObservable>().wrappedValue

        cancellable = cacheObservation.usersPublisher
            .synchroNodesPublisher(for: synchroDbId)
            .receive(on: DispatchQueue.main)
            .sink { [weak self] nodes in
                self?.wrappedValue = nodes
            }
    }

    deinit { cancellable?.cancel() }

    var projectedValue: ObservedSynchroNodes {
        self
    }
}
