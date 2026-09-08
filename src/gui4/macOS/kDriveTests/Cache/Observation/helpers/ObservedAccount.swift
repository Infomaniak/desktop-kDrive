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
