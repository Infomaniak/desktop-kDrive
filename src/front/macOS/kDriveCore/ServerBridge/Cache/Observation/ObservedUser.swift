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

// TODO: Extend observation with deletion _after_ signals are created
public enum ObservationEvent<Some: Equatable>: Equatable {
    case update(Some)
    case removed
}

@propertyWrapper
public final class ObservedUser: ObservableObject {
    @Published public private(set) var wrappedValue: User?
    private var cancellable: AnyCancellable?

    public init(dbId: Int32, cacheObservation: CoherentCacheObservable? = nil) {
        let cacheObservation = cacheObservation ?? InjectService<CoherentCacheObservable>().wrappedValue

        cancellable = cacheObservation.usersPublisher
            .userPublisher(for: dbId)
            .receive(on: DispatchQueue.main)
            .sink { [weak self] user in
                self?.wrappedValue = user
            }
    }

    deinit { cancellable?.cancel() }

    public var projectedValue: ObservedUser { self }
}

public extension AnyPublisher where Output == IndexedUsers, Failure == Never {
    func userPublisher(for dbId: Int32) -> AnyPublisher<User?, Never> {
        map { $0[dbId] }
            .scan((old: User?.none, new: User?.none)) { prev, newUser in
                (old: prev.new, new: newUser)
            }
            .map { pair -> User? in
                guard pair.old != pair.new else { return nil }
                return pair.new
            }
            .compactMap { $0 }
            .eraseToAnyPublisher()
    }
}
