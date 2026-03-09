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
import kDriveCore
import OrderedCollections

@MainActor
@propertyWrapper
final class ObservedDrive: ObservableObject {
    @Published private(set) var wrappedValue: Drive?
    private var cancellable: AnyCancellable?

    init(
        userDbId: Int32,
        accountDbId: Int32,
        driveDbId: Int32,
        cacheObservation: CoherentCacheObservable? = nil
    ) {
        let cacheObservation =
            cacheObservation ?? InjectService<CoherentCacheObservable>().wrappedValue

        cancellable = cacheObservation.usersPublisher
            .drivePublisher(
                userDbId: userDbId,
                accountDbId: accountDbId,
                driveDbId: driveDbId
            )
            .receive(on: DispatchQueue.main)
            .sink { [weak self] drive in
                self?.wrappedValue = drive
            }
    }

    init(
        driveDbId: Int32,
        cacheObservation: CoherentCacheObservable? = nil
    ) {
        let cacheObservation =
            cacheObservation ?? InjectService<CoherentCacheObservable>().wrappedValue

        cancellable = cacheObservation.usersPublisher
            .drivePublisher(driveDbId: driveDbId)
            .receive(on: DispatchQueue.main)
            .sink { [weak self] drive in
                self?.wrappedValue = drive
            }
    }

    deinit { cancellable?.cancel() }

    var projectedValue: ObservedDrive {
        self
    }
}
