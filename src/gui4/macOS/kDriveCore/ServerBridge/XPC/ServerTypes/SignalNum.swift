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

public enum SignalNum: Int {
    case Unknown = 0
    // User
    case USER_ADDED
    case USER_UPDATED
    case USER_STATUSCHANGED
    case USER_REMOVED
    // Account
    case ACCOUNT_ADDED
    case ACCOUNT_UPDATED
    case ACCOUNT_REMOVED
    // Drive
    case DRIVE_ADDED
    case DRIVE_UPDATED
    case DRIVE_QUOTAUPDATED_LEGACY
    case DRIVE_REMOVED
    case DRIVE_DELETE_FAILED
    // Sync
    case SYNC_ADDED
    case SYNC_UPDATED
    case SYNC_REMOVED
    case SYNC_PROGRESSINFO
    case SYNC_COMPLETEDITEM
    case SYNC_VFS_CONVERSION_COMPLETED
    case SYNC_DELETE_FAILED
    // Node
    case NODE_FOLDER_SIZE_COMPLETED
    case NODE_FIX_CONFLICTED_FILES_COMPLETED
    // Update
    case UPDATER_SHOW_DIALOG
    case UPDATER_STATE_CHANGED
    // Login
    case LOGIN_SEND_AUTHORIZATION_CODE
    // Utility
    case UTILITY_SHOW_NOTIFICATION
    case UTILITY_ERROR_ADDED_LEGACY
    case UTILITY_ERROR_ADDED
    case UTILITY_ERROR_REMOVED
    case UTILITY_ERRORS_CLEARED
    case UTILITY_SHOW_SETTINGS
    case UTILITY_SHOW_SYNTHESIS
    case UTILITY_LOG_UPLOAD_STATUS_UPDATED
    case UTILITY_QUIT
    case EnumEnd
}
