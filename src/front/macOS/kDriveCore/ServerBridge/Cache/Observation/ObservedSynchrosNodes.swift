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
public final class ObservedSynchrosNodes: ObservableObject {
    @Published public private(set) var wrappedValue: [SynchroNodeContext] = []
    private var cancellable: AnyCancellable?

    public init(
        cacheObservation: CoherentCacheObservable? = nil
    ) {
        let cacheObservation =
            cacheObservation ?? InjectService<CoherentCacheObservable>().wrappedValue

        cancellable = cacheObservation.usersPublisher
            .allSynchroNodesPublisher()
            .receive(on: DispatchQueue.main)
            .sink { [weak self] nodes in
                self?.wrappedValue = nodes
            }
    }

    deinit { cancellable?.cancel() }

    public var projectedValue: ObservedSynchrosNodes {
        self
    }
}

public struct SynchroNodeContext: Sendable, Equatable {
    public let node: SynchroNode
    public let synchro: Synchro
    public let drive: Drive
    public let account: Account
    public let user: User
}

public extension AnyPublisher where Output == IndexedUsers, Failure == Never {
    func allSynchroNodesPublisher() -> AnyPublisher<[SynchroNodeContext], Never> {
        map { usersDict in
            var allNodes: [SynchroNodeContext] = []

            for user in usersDict.values {
                for account in user.accounts.values {
                    for drive in account.drives.values {
                        for synchro in drive.synchros.values {
                            for node in synchro.synchNodes.values {
                                let context = SynchroNodeContext(
                                    node: node,
                                    synchro: synchro,
                                    drive: drive,
                                    account: account,
                                    user: user
                                )
                                allNodes.append(context)
                            }
                        }
                    }
                }
            }

            return allNodes.sorted { $0.node.date > $1.node.date }
        }
        .removeDuplicates()
        .eraseToAnyPublisher()
    }
}
