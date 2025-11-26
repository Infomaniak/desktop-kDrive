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
public final class ObservedAccount: ObservableObject {
    @Published public private(set) var wrappedValue: Account?

    private var cancellable: AnyCancellable?

    public init(userDbId: Int32, accountDbId: Int32, cacheObservation: CoherentCacheObservable? = nil) {
        let cacheObservation = cacheObservation ?? InjectService<CoherentCacheObservable>().wrappedValue
        let accountsPublisher = cacheObservation.accountsPublisher

        cancellable = accountsPublisher
            .accountPublisher(forUserDbId: userDbId, accountDbId: accountDbId)
            .receive(on: DispatchQueue.main)
            .assign(to: \.wrappedValue, onWeak: self)
    }

    deinit {
        cancellable?.cancel()
    }

    public var projectedValue: ObservedAccount { self }
}

extension AnyPublisher where Output == UserAccounts, Failure == Never {
    func accountPublisher(forUserDbId userDbId: Int32, accountDbId: Int32) -> AnyPublisher<Account?, Never> {
        map { userAccounts in
            let receivedUserDbId = userAccounts.0
            let receivedAccounts = userAccounts.1
            guard receivedUserDbId == userDbId else { return nil }
            guard let account = receivedAccounts[accountDbId] else { return nil }
            return account
        }
        .removeDuplicates { $0 == $1 }
        .eraseToAnyPublisher()
    }
}
