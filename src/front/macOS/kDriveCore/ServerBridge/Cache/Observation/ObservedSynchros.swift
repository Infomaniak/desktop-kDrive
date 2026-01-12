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

@MainActor
@propertyWrapper
public final class ObservedSynchros: ObservableObject {
    @Published public private(set) var wrappedValue: [SynchroContext] = []
    private var cancellable: AnyCancellable?

    public init(
        cacheObservation: CoherentCacheObservable? = nil
    ) {
        let cacheObservation =
            cacheObservation ?? InjectService<CoherentCacheObservable>().wrappedValue

        cancellable = cacheObservation.usersPublisher
            .allSynchrosPublisher()
            .receive(on: DispatchQueue.main)
            .sink { [weak self] synchros in
                self?.wrappedValue = synchros
            }
    }

    deinit { cancellable?.cancel() }

    public var projectedValue: ObservedSynchros { self }
}

public extension AnyPublisher where Output == IndexedUsers, Failure == Never {
    func allSynchrosPublisher() -> AnyPublisher<[SynchroContext], Never> {
        map { usersDict in
            usersDict.values.flatMap { user in
                user.accounts.values.flatMap { account in
                    account.drives.values.flatMap { drive in
                        drive.synchros.values.map {
                            SynchroContext(synchro: $0, drive: drive, account: account, user: user)
                        }
                    }
                }
            }
        }
        .removeDuplicates()
        .eraseToAnyPublisher()
    }
}
