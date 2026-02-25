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
import OrderedCollections

public struct UIIndexedSynchroContextOptions: OptionSet {
    public let rawValue: Int

    public static let progressInfo = UIIndexedSynchroContextOptions(rawValue: 1 << 1)

    public init(rawValue: Int) {
        self.rawValue = rawValue
    }
}

public extension Published.Publisher where Value == UIIndexedSynchroContext {
    func removeSynchroContextDuplicates(with options: UIIndexedSynchroContextOptions) -> Publishers.RemoveDuplicates<Self> {
        removeDuplicates { lhs, rhs in
            guard lhs.keys == rhs.keys else {
                return false
            }

            for key in lhs.keys {
                guard let lhsContext = lhs[key], let rhsContext = rhs[key] else {
                    return false
                }

                if lhsContext.drive != rhsContext.drive
                    || lhsContext.account != rhsContext.account
                    || lhsContext.user != rhsContext.user
                    || lhsContext.blockingError != rhsContext.blockingError {
                    return false
                }

                let lhsSynchro = lhsContext.synchro
                let rhsSynchro = rhsContext.synchro

                if lhsSynchro.dbId != rhsSynchro.dbId
                    || lhsSynchro.driveDbId != rhsSynchro.driveDbId
                    || lhsSynchro.localPath != rhsSynchro.localPath
                    || lhsSynchro.errorCount != rhsSynchro.errorCount {
                    return false
                }

                if options.contains(.progressInfo) && lhsSynchro.progressInfo != rhsSynchro.progressInfo {
                    return false
                }
            }

            return true
        }
    }
}
