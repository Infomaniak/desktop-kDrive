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
import OrderedCollections

public extension AnyPublisher where Output == IndexedUsers, Failure == Never {
    func synchroEventPublisher(
        userDbId: Int32,
        accountDbId: Int32,
        driveDbId: Int32,
        synchroDbId: Int32
    ) -> AnyPublisher<ObservationEvent<Synchro>, Never> {
        map { usersDict -> Synchro? in
            usersDict[userDbId]?
                .accounts[accountDbId]?
                .drives[driveDbId]?
                .synchros[synchroDbId]
        }
        .map { synchro in
            synchro.map(ObservationEvent.update) ?? .removed
        }
        .removeDuplicates()
        .eraseToAnyPublisher()
    }

    func synchroEventPublisher(synchroDbId: Int32) -> AnyPublisher<ObservationEvent<Synchro>, Never> {
        map { usersDict -> Synchro? in
            for user in usersDict.values {
                for account in user.accounts.values {
                    for drive in account.drives.values {
                        if let synchro = drive.synchros[synchroDbId] {
                            return synchro
                        }
                    }
                }
            }
            return nil
        }
        .map { synchro in
            synchro.map(ObservationEvent.update) ?? .removed
        }
        .removeDuplicates()
        .eraseToAnyPublisher()
    }

    func synchroPublisher(
        userDbId: Int32,
        accountDbId: Int32,
        driveDbId: Int32,
        synchroDbId: Int32
    ) -> AnyPublisher<Synchro?, Never> {
        synchroEventPublisher(
            userDbId: userDbId,
            accountDbId: accountDbId,
            driveDbId: driveDbId,
            synchroDbId: synchroDbId
        )
        .map { event -> Synchro? in
            switch event {
            case .update(let synchro): return synchro
            case .removed: return nil
            }
        }
        .removeDuplicates()
        .eraseToAnyPublisher()
    }

    func synchroPublisher(dbId synchroDbId: Int32) -> AnyPublisher<Synchro?, Never> {
        synchroEventPublisher(synchroDbId: synchroDbId)
            .map { event -> Synchro? in
                switch event {
                case .update(let synchro): return synchro
                case .removed: return nil
                }
            }
            .removeDuplicates()
            .eraseToAnyPublisher()
    }
}
