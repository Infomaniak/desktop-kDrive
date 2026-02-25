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

public struct AvailableDriveContext: Sendable, Equatable {
    public let availableDrive: AvailableDrive
    public let user: User
}

@MainActor
@propertyWrapper
public final class ObservedAvailableDrives: ObservableObject {
    @Published public private(set) var wrappedValue: [AvailableDriveContext] = []
    private var cancellable: AnyCancellable?

    public init(
        cacheObservation: CoherentCacheObservable? = nil
    ) {
        let cacheObservation =
            cacheObservation ?? InjectService<CoherentCacheObservable>().wrappedValue

        cancellable = cacheObservation.usersPublisher
            .allAvailableDrivesPublisher()
            .receive(on: DispatchQueue.main)
            .sink { [weak self] availableDrives in
                self?.wrappedValue = availableDrives
            }
    }

    deinit { cancellable?.cancel() }

    public var projectedValue: ObservedAvailableDrives { self }
}

public extension AnyPublisher where Output == IndexedUsers, Failure == Never {
    func allAvailableDrivesPublisher() -> AnyPublisher<[AvailableDriveContext], Never> {
        map { usersDict in
            usersDict.values.flatMap { user in
                user.availableDrives.values.compactMap { availableDrive in
                    AvailableDriveContext(availableDrive: availableDrive, user: user)
                }
            }
        }
        .removeDuplicates()
        .eraseToAnyPublisher()
    }
}
