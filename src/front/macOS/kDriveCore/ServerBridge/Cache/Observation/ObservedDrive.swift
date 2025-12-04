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
public final class ObservedDrive: ObservableObject {
    @Published public private(set) var wrappedValue: Drive?
    private var cancellable: AnyCancellable?

    public init(
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

    public init(
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

    public var projectedValue: ObservedDrive { self }
}

public extension AnyPublisher where Output == IndexedUsers, Failure == Never {
    func driveEventPublisher(
        userDbId: Int32,
        accountDbId: Int32,
        driveDbId: Int32
    ) -> AnyPublisher<ObservationEvent<Drive>, Never> {
        map { usersDict -> Drive? in
            usersDict[userDbId]?
                .accounts[accountDbId]?
                .drives[driveDbId]
        }
        .map { drive -> ObservationEvent<Drive> in
            if let drive { return .update(drive) }
            return .removed
        }
        .removeDuplicates()
        .eraseToAnyPublisher()
    }

    func driveEventPublisher(driveDbId: Int32) -> AnyPublisher<ObservationEvent<Drive>, Never> {
        map { usersDict -> Drive? in
            for user in usersDict.values {
                for account in user.accounts.values {
                    if let drive = account.drives[driveDbId] {
                        return drive
                    }
                }
            }
            return nil
        }
        .map { drive -> ObservationEvent<Drive> in
            drive.map(ObservationEvent.update) ?? .removed
        }
        .removeDuplicates()
        .eraseToAnyPublisher()
    }

    func drivePublisher(
        userDbId: Int32,
        accountDbId: Int32,
        driveDbId: Int32
    ) -> AnyPublisher<Drive?, Never> {
        driveEventPublisher(userDbId: userDbId, accountDbId: accountDbId, driveDbId: driveDbId)
            .map { event -> Drive? in
                switch event {
                case let .update(drive): return drive
                case .removed: return nil
                }
            }
            .removeDuplicates()
            .eraseToAnyPublisher()
    }

    func drivePublisher(driveDbId: Int32) -> AnyPublisher<Drive?, Never> {
        driveEventPublisher(driveDbId: driveDbId)
            .map { event -> Drive? in
                switch event {
                case let .update(drive): return drive
                case .removed: return nil
                }
            }
            .removeDuplicates()
            .eraseToAnyPublisher()
    }
}
