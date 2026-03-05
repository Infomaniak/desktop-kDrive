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

public struct DriveContext: Sendable, Equatable {
    public let drive: Drive
    public let account: Account
    public let user: User
}

@MainActor
@propertyWrapper
final class ObservedDrives: ObservableObject {
    @Published public private(set) var wrappedValue: [DriveContext] = []
    private var cancellable: AnyCancellable?

    public init(
        cacheObservation: CoherentCacheObservable? = nil
    ) {
        let cacheObservation =
            cacheObservation ?? InjectService<CoherentCacheObservable>().wrappedValue

        cancellable = cacheObservation.usersPublisher
            .allDrivesPublisher()
            .receive(on: DispatchQueue.main)
            .sink { [weak self] drives in
                self?.wrappedValue = drives
            }
    }

    deinit { cancellable?.cancel() }

    public var projectedValue: ObservedDrives { self }
}

public extension AnyPublisher where Output == IndexedUsers, Failure == Never {
    func allDrivesPublisher() -> AnyPublisher<[DriveContext], Never> {
        map { usersDict in
            usersDict.values.flatMap { user in
                user.accounts.values.flatMap { account in
                    account.drives.values.map { drive in
                        DriveContext(drive: drive, account: account, user: user)
                    }
                }
            }
        }
        .removeDuplicates()
        .eraseToAnyPublisher()
    }
}
