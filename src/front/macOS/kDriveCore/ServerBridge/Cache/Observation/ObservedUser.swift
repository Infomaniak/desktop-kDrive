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

@propertyWrapper
public final class ObservedUser: ObservableObject {
    @Published public private(set) var wrappedValue: User?

    private var cancellable: AnyCancellable?

    public init(dbId: Int32, cacheObservation: CoherentCacheObservation? = nil) {
        let cacheObservation = cacheObservation ?? InjectService<CoherentCacheObservation>().wrappedValue
        let usersPublisher = cacheObservation.usersPublisher

        cancellable = usersPublisher
            .userPublisher(for: dbId)
            .receive(on: DispatchQueue.main)
            .assign(to: \.wrappedValue, onWeak: self)
    }

    deinit {
        cancellable?.cancel()
    }

    public var projectedValue: ObservedUser { self }
}

extension AnyPublisher where Output == IndexedUsers, Failure == Never {
    func userPublisher(for id: Int32) -> AnyPublisher<User?, Never> {
        map { $0[id] }
            .removeDuplicates { $0 == $1 }
            .eraseToAnyPublisher()
    }
}

private extension Publisher {
    /// Assigns values to a property on a weakly captured object to avoid retain cycles
    func assign<Root: AnyObject>(
        to keyPath: ReferenceWritableKeyPath<Root, Output>,
        onWeak object: Root
    ) -> AnyCancellable where Failure == Never {
        sink { [weak object] value in
            object?[keyPath: keyPath] = value
        }
    }
}
