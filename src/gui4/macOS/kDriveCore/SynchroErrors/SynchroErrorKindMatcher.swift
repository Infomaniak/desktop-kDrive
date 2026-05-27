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

import Foundation

struct SynchroErrorKindMatcher {
    var levels: Set<KDC.ErrorLevel>
    var nodeTypes: Set<KDC.NodeType>
    var cancelTypes: Set<KDC.CancelType>
    var inconsistencyTypes: Set<KDC.InconsistencyType>
    var conflictTypes: Set<KDC.ConflictType>
    var exitCodes: Set<KDC.ExitCode>
    var exitCauses: Set<KDC.ExitCause>

    init(
        levels: Set<KDC.ErrorLevel>? = nil,
        nodeTypes: Set<KDC.NodeType>? = nil,
        cancelTypes: Set<KDC.CancelType>? = nil,
        inconsistencyTypes: Set<KDC.InconsistencyType>? = nil,
        conflictTypes: Set<KDC.ConflictType>? = nil,
        exitCodes: Set<KDC.ExitCode>? = nil,
        exitCauses: Set<KDC.ExitCause>? = nil
    ) {
        self.levels = levels ?? [.Unknown]
        self.nodeTypes = nodeTypes ?? [.Unknown]
        self.cancelTypes = cancelTypes ?? [.None]
        self.inconsistencyTypes = inconsistencyTypes ?? [.None]
        self.conflictTypes = conflictTypes ?? [.None]
        self.exitCodes = exitCodes ?? [.Unknown]
        self.exitCauses = exitCauses ?? [.Unknown]
    }

    func matches(error: ErrorInfo) -> Bool {
        return levels.contains(error.level)
            && nodeTypes.contains(error.nodeType)
            && cancelTypes.contains(error.cancelType)
            && inconsistencyTypes.contains(error.inconsistencyType)
            && conflictTypes.contains(error.conflictType)
            && exitCodes.contains(error.exitCode)
            && exitCauses.contains(error.exitCause)
    }
}

extension SynchroErrorKindMatcher {
    static func node(
        cancelTypes: Set<KDC.CancelType>? = nil,
        inconsistencyTypes: Set<KDC.InconsistencyType>? = nil,
        conflictTypes: Set<KDC.ConflictType>? = nil,
        exitCodes: Set<KDC.ExitCode>? = nil,
        exitCauses: Set<KDC.ExitCause>? = nil
    ) -> SynchroErrorKindMatcher {
        return SynchroErrorKindMatcher(
            levels: [.Node],
            nodeTypes: [.File, .Directory],
            cancelTypes: cancelTypes,
            inconsistencyTypes: inconsistencyTypes,
            conflictTypes: conflictTypes,
            exitCodes: exitCodes,
            exitCauses: exitCauses
        )
    }

    static func syncPal(
        nodeTypes: Set<KDC.NodeType>? = nil,
        cancelTypes: Set<KDC.CancelType>? = nil,
        inconsistencyTypes: Set<KDC.InconsistencyType>? = nil,
        conflictTypes: Set<KDC.ConflictType>? = nil,
        exitCodes: Set<KDC.ExitCode>? = nil,
        exitCauses: Set<KDC.ExitCause>? = nil
    ) -> SynchroErrorKindMatcher {
        return SynchroErrorKindMatcher(
            levels: [.SyncPal],
            nodeTypes: nodeTypes,
            cancelTypes: cancelTypes,
            inconsistencyTypes: inconsistencyTypes,
            conflictTypes: conflictTypes,
            exitCodes: exitCodes,
            exitCauses: exitCauses
        )
    }
}
