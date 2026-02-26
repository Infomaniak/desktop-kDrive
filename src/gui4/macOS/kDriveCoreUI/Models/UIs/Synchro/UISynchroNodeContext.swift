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

import Foundation
import kDriveCore

public struct UISynchroNodeContext: Sendable, Identifiable, Equatable {
    public var id: UISynchroNode.ID {
        node.id
    }

    public let user: UIUser
    public let account: UIAccount
    public let drive: UIDrive
    public let synchro: UISynchro
    public let node: UISynchroNode

    public init(user: UIUser, account: UIAccount, drive: UIDrive, synchro: UISynchro, node: UISynchroNode) {
        self.user = user
        self.account = account
        self.drive = drive
        self.synchro = synchro
        self.node = node
    }
}

public extension UISynchroNodeContext {
    init(synchroNodeContext: SynchroNodeContext) {
        self.init(
            user: UIUser(user: synchroNodeContext.user),
            account: UIAccount(account: synchroNodeContext.account),
            drive: UIDrive(drive: synchroNodeContext.drive),
            synchro: UISynchro(synchro: synchroNodeContext.synchro),
            node: UISynchroNode(synchroNode: synchroNodeContext.node)
        )
    }
}
