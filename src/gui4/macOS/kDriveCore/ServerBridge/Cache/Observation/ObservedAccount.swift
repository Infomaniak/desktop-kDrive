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

@MainActor
@propertyWrapper
final class ObservedAccount: ObservableObject {
    @Published private(set) var wrappedValue: Account?
    private var cancellable: AnyCancellable?

    init(
        userDbId: Int32,
        accountDbId: Int32,
        cacheObservation: CoherentCacheObservable? = nil
    ) {
        let cacheObservation =
            cacheObservation ?? InjectService<CoherentCacheObservable>().wrappedValue

        cancellable = cacheObservation.usersPublisher
            .accountPublisher(userDbId: userDbId, accountDbId: accountDbId)
            .receive(on: DispatchQueue.main)
            .sink { [weak self] account in
                self?.wrappedValue = account
            }
    }

    init(
        accountDbId: Int32,
        cacheObservation: CoherentCacheObservable? = nil
    ) {
        let cacheObservation =
            cacheObservation ?? InjectService<CoherentCacheObservable>().wrappedValue

        cancellable = cacheObservation.usersPublisher
            .accountPublisher(accountDbId: accountDbId)
            .receive(on: DispatchQueue.main)
            .sink { [weak self] account in
                self?.wrappedValue = account
            }
    }

    deinit { cancellable?.cancel() }

    var projectedValue: ObservedAccount {
        self
    }
}

public extension AnyPublisher where Output == IndexedUsers, Failure == Never {
    func accountEventPublisher(
        userDbId: Int32,
        accountDbId: Int32
    ) -> AnyPublisher<ObservationEvent<Account>, Never> {
        map { usersDict -> Account? in
            usersDict[userDbId]?.accounts[accountDbId]
        }
        .map { account in
            account.map(ObservationEvent.update) ?? .removed
        }
        .removeDuplicates()
        .eraseToAnyPublisher()
    }

    func accountEventPublisher(
        accountDbId: Int32
    ) -> AnyPublisher<ObservationEvent<Account>, Never> {
        map { usersDict -> Account? in
            for user in usersDict.values {
                if let account = user.accounts[accountDbId] {
                    return account
                }
            }
            return nil
        }
        .map { account in
            account.map(ObservationEvent.update) ?? .removed
        }
        .removeDuplicates()
        .eraseToAnyPublisher()
    }

    func accountPublisher(
        userDbId: Int32,
        accountDbId: Int32
    ) -> AnyPublisher<Account?, Never> {
        accountEventPublisher(
            userDbId: userDbId,
            accountDbId: accountDbId
        )
        .map { event -> Account? in
            switch event {
            case .update(let account): return account
            case .removed: return nil
            }
        }
        .removeDuplicates()
        .eraseToAnyPublisher()
    }

    func accountPublisher(
        accountDbId: Int32
    ) -> AnyPublisher<Account?, Never> {
        accountEventPublisher(accountDbId: accountDbId)
            .map { event -> Account? in
                switch event {
                case .update(let account): return account
                case .removed: return nil
                }
            }
            .removeDuplicates()
            .eraseToAnyPublisher()
    }
}
