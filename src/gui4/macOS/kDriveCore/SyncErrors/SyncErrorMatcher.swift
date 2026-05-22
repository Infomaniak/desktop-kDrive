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

struct SyncErrorMatcher {
    var levels: Set<KDC.ErrorLevel> = [.Unknown]
    var nodeTypes: Set<KDC.NodeType> = [.Unknown]
    var cancelTypes: Set<KDC.CancelType> = [.None]
    var inconsistencyTypes: Set<KDC.InconsistencyType> = [.None]
    var conflictTypes: Set<KDC.ConflictType> = [.None]
    var exitCodes: Set<KDC.ExitCode> = [.Unknown]
    var exitCauses: Set<KDC.ExitCause> = [.Unknown]

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
