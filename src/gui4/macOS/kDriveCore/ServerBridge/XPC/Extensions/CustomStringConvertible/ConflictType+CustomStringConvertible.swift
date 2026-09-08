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

import CppInterop
import Foundation

extension KDC.ConflictType: CustomStringConvertible {
    public var description: String {
        switch self {
        case .None:
            return "None"
        case .EditDelete:
            return "EditDelete"
        case .MoveDelete:
            return "MoveDelete"
        case .MoveParentDelete:
            return "MoveParentDelete"
        case .CreateParentDelete:
            return "CreateParentDelete"
        case .MoveMoveSource:
            return "MoveMoveSource"
        case .MoveMoveDest:
            return "MoveMoveDest"
        case .MoveCreate:
            return "MoveCreate"
        case .CreateCreate:
            return "CreateCreate"
        case .EditEdit:
            return "EditEdit"
        case .MoveMoveCycle:
            return "MoveMoveCycle"
        case .EnumEnd:
            return "EnumEnd"
        @unknown default:
            return "ConflictType(rawValue: \(rawValue))"
        }
    }
}
