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

extension KDC.ExitCode: CustomStringConvertible {
    public var description: String {
        switch self {
        case .Ok:
            return "Ok"
        case .Unknown:
            return "Unknown"
        case .NetworkError:
            return "NetworkError"
        case .InvalidToken:
            return "InvalidToken"
        case .DataError:
            return "DataError"
        case .DbError:
            return "DbError"
        case .BackError:
            return "BackError"
        case .SystemError:
            return "SystemError"
        case .FatalError:
            return "FatalError"
        case .LogicError:
            return "LogicError"
        case .TokenRefreshed:
            return "TokenRefreshed"
        case .RateLimited:
            return "RateLimited"
        case .InvalidSync:
            return "InvalidSync"
        case .InvalidOperation:
            return "InvalidOperation"
        case .OperationCanceled:
            return "OperationCanceled"
        case .UpdateRequired:
            return "UpdateRequired"
        case .LogUploadFailed:
            return "LogUploadFailed"
        case .UpdateFailed:
            return "UpdateFailed"
        case .EnumEnd:
            return "EnumEnd"
        @unknown default:
            return "ExitCode(rawValue: \(rawValue))"
        }
    }
}
