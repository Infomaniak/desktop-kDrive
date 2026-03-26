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

public enum RequestNum: Int {
    case Unknown = 0
    case LOGIN_REQUESTTOKEN
    case USER_DBIDLIST
    case USER_INFOLIST
    case USER_DELETE
    case USER_AVAILABLEDRIVES
    case ACCOUNT_INFOLIST
    case DRIVE_INFOLIST
    case DRIVE_UPDATE
    case DRIVE_DELETE
    case DRIVE_SEARCH
    case SYNC_INFOLIST
    case SYNC_OFFLINE_FILES_SIZE
    case SYNC_START
    case SYNC_STOP
    case SYNC_STATUS
    case SYNC_ADD
    case SYNC_ADD2
    case SYNC_START_AFTER_LOGIN
    case SYNC_DELETE
    case SYNC_GETPUBLICLINKURL
    case SYNC_GETPRIVATELINKURL
    case SYNC_TRIGGER_PROGRESS_UPDATE
    case SYNC_SETSUPPORTSVIRTUALFILES
    case BLACKLISTED_NODE_LIST
    case BLACKLISTED_NODE_SETLIST
    case NODE_PATH
    case NODE_INFO
    case NODE_SUBFOLDERS
    case NODE_SUBFOLDERS2
    case NODE_FOLDER_SIZE
    case NODE_CREATEMISSINGFOLDERS
    case NODE_CREATEMISSINGFOLDERS_LEGACY
    case NODE_CONFLICT_INFO
    case ERROR_INFOLIST
    case ERROR_INFOLIST_LEGACY
    case ERROR_GET_CONFLICTS_LEGACY
    case ERROR_DELETE_SERVER
    case ERROR_DELETE_SYNC
    case ERROR_DELETE_INVALIDTOKEN
    case ERROR_RESOLVE_CONFLICTS_LEGACY
    case ERROR_RESOLVE_CONFLICTS
    case ERROR_RESOLVE_CONFLICTS_QUICK
    case ERROR_RESOLVE_UNSUPPORTED_CHAR_LEGACY
    case EXCLTEMPL_GETEXCLUDED
    case EXCLTEMPL_GETLIST
    case EXCLTEMPL_SETUSERLIST
    case EXCLAPP_GETLIST
    case EXCLAPP_SETLIST
    case EXCLAPP_GET_FETCHING_APP_LIST
    case PARAMETERS_INFO
    case PARAMETERS_UPDATE
    case UTILITY_BESTVFSAVAILABLEMODE
    case UTILITY_BESTVFSAVAILABLEMODE_LEGACY
    case UTILITY_FINDGOODPATHFORNEWSYNC
    case UTILITY_ISPATHVALIDFORNEWSYNC
    case UTILITY_ACTIVATELOADINFO
    case UTILITY_CHECKCOMMSTATUS
    case UTILITY_HASSYSTEMLAUNCHONSTARTUP
    case UTILITY_HASLAUNCHONSTARTUP
    case UTILITY_SETLAUNCHONSTARTUP
    case UTILITY_SET_APPSTATE
    case UTILITY_GET_APPSTATE
    case UTILITY_SEND_LOG_TO_SUPPORT
    case UTILITY_CANCEL_LOG_TO_SUPPORT
    case UTILITY_GET_LOG_ESTIMATED_SIZE_LEGACY // Not used anymore but kept for backward compatibility
    case UTILITY_CRASH
    case UTILITY_QUIT
    case UTILITY_SEND_APP_START_TRACE // Sent by the Client process as soon the UI is visible for the user
    case UPDATER_CHANGE_CHANNEL
    case UPDATER_VERSION_INFO
    case UPDATER_STATE
    case UPDATER_START_INSTALLER
    case UPDATER_SKIP_VERSION
    case EnumEnd
}
