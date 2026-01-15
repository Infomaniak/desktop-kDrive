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

struct ErrorInfoSignal: Codable, Sendable {
    let dbId: Int32
    let syncDbId: Int32
    let time: Int64
    let level: KDC.ErrorLevel
    @Base64CodedString var functionName: String
    @Base64CodedString var workerName: String
    let exitCode: KDC.ExitCode
    let exitCause: KDC.ExitCause
    @Base64CodedString var localNodeId: String
    @Base64CodedString var remoteNodeId: String
    let nodeType: KDC.NodeType
    @Base64CodedString var path: String
    let conflictType: KDC.ConflictType
    let inconsistencyType: KDC.InconsistencyType
    let cancelType: KDC.CancelType
    @Base64CodedString var destinationPath: String
    let autoResolved: Bool
}
