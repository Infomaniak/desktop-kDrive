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
import Foundation
import InfomaniakDI
import OrderedCollections

public enum ObservationEvent<Some: Equatable>: Equatable {
    case update(Some)
    case removed
}

@MainActor
@propertyWrapper
final class ObservedUser: ObservableObject {
    @Published private(set) var wrappedValue: User?
    private var cancellable: AnyCancellable?

    init(userDbId: Int32, cacheObservation: CoherentCacheObservable? = nil) {
        let cacheObservation = cacheObservation ?? InjectService<CoherentCacheObservable>().wrappedValue

        cancellable = cacheObservation.usersPublisher
            .userPublisher(userDbId: userDbId)
            .receive(on: DispatchQueue.main)
            .sink { [weak self] user in
                self?.wrappedValue = user
            }
    }

    deinit { cancellable?.cancel() }

    var projectedValue: ObservedUser {
        self
    }
}

public extension AnyPublisher where Output == IndexedUsers, Failure == Never {
    func userEventPublisher(userDbId: Int32) -> AnyPublisher<ObservationEvent<User>, Never> {
        map { usersDict -> User? in
            usersDict[userDbId]
        }
        .map { account in
            account.map(ObservationEvent.update) ?? .removed
        }
        .removeDuplicates()
        .eraseToAnyPublisher()
    }

    func userPublisher(userDbId: Int32) -> AnyPublisher<User?, Never> {
        userEventPublisher(userDbId: userDbId)
            .map { event -> User? in
                switch event {
                case .update(let user): return user
                case .removed: return nil
                }
            }
            .removeDuplicates()
            .eraseToAnyPublisher()
    }
}
