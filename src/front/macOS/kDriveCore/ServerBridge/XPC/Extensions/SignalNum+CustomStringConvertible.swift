//
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

extension SignalNum: CustomStringConvertible {
    public var description: String {
        switch self {
        case .Unknown:
            return "Unknown"
        case .USER_ADDED:
            return "USER_ADDED"
        case .USER_UPDATED:
            return "USER_UPDATED"
        case .USER_STATUSCHANGED:
            return "USER_STATUSCHANGED"
        case .USER_REMOVED:
            return "USER_REMOVED"
        case .ACCOUNT_ADDED:
            return "ACCOUNT_ADDED"
        case .ACCOUNT_UPDATED:
            return "ACCOUNT_UPDATED"
        case .ACCOUNT_REMOVED:
            return "ACCOUNT_REMOVED"
        case .DRIVE_ADDED:
            return "DRIVE_ADDED"
        case .DRIVE_UPDATED:
            return "DRIVE_UPDATED"
        case .DRIVE_QUOTAUPDATED:
            return "DRIVE_QUOTAUPDATED"
        case .DRIVE_REMOVED:
            return "DRIVE_REMOVED"
        case .DRIVE_DELETE_FAILED:
            return "DRIVE_DELETE_FAILED"
        case .SYNC_ADDED:
            return "SYNC_ADDED"
        case .SYNC_UPDATED:
            return "SYNC_UPDATED"
        case .SYNC_REMOVED:
            return "SYNC_REMOVED"
        case .SYNC_PROGRESSINFO:
            return "SYNC_PROGRESSINFO"
        case .SYNC_COMPLETEDITEM:
            return "SYNC_COMPLETEDITEM"
        case .SYNC_VFS_CONVERSION_COMPLETED:
            return "SYNC_VFS_CONVERSION_COMPLETED"
        case .SYNC_DELETE_FAILED:
            return "SYNC_DELETE_FAILED"
        case .NODE_FOLDER_SIZE_COMPLETED:
            return "NODE_FOLDER_SIZE_COMPLETED"
        case .NODE_FIX_CONFLICTED_FILES_COMPLETED:
            return "NODE_FIX_CONFLICTED_FILES_COMPLETED"
        case .UPDATER_SHOW_DIALOG:
            return "UPDATER_SHOW_DIALOG"
        case .UPDATER_STATE_CHANGED:
            return "UPDATER_STATE_CHANGED"
        case .UTILITY_SHOW_NOTIFICATION:
            return "UTILITY_SHOW_NOTIFICATION"
        case .UTILITY_ERROR_ADDED_LEGACY:
            return "UTILITY_ERROR_ADDED_LEGACY"
        case .UTILITY_ERROR_ADDED:
            return "UTILITY_ERROR_ADDED"
        case .UTILITY_ERROR_REMOVED:
            return "UTILITY_ERROR_REMOVED"
        case .UTILITY_ERRORS_CLEARED:
            return "UTILITY_ERRORS_CLEARED"
        case .UTILITY_SHOW_SETTINGS:
            return "UTILITY_SHOW_SETTINGS"
        case .UTILITY_SHOW_SYNTHESIS:
            return "UTILITY_SHOW_SYNTHESIS"
        case .UTILITY_LOG_UPLOAD_STATUS_UPDATED:
            return "UTILITY_LOG_UPLOAD_STATUS_UPDATED"
        case .UTILITY_QUIT:
            return "UTILITY_QUIT"
        case .EnumEnd:
            return "EnumEnd"
        default:
            return "SignalNum(rawValue: \(rawValue))"
        }
    }
}
