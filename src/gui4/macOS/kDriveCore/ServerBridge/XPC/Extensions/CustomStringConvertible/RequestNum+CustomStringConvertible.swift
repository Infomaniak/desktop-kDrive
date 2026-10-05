/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import CppInterop
import Foundation

extension RequestNum: CustomStringConvertible {
    public var description: String {
        switch self {
        case .Unknown:
            return "Unknown"
        case .LOGIN_REQUESTTOKEN:
            return "LOGIN_REQUESTTOKEN"
        case .USER_DBIDLIST:
            return "USER_DBIDLIST"
        case .USER_INFOLIST:
            return "USER_INFOLIST"
        case .USER_DELETE:
            return "USER_DELETE"
        case .USER_AVAILABLEDRIVES:
            return "USER_AVAILABLEDRIVES"
        case .ACCOUNT_INFOLIST:
            return "ACCOUNT_INFOLIST"
        case .DRIVE_INFOLIST:
            return "DRIVE_INFOLIST"
        case .DRIVE_UPDATE:
            return "DRIVE_UPDATE"
        case .DRIVE_DELETE:
            return "DRIVE_DELETE"
        case .DRIVE_SEARCH:
            return "DRIVE_SEARCH"
        case .SYNC_INFOLIST:
            return "SYNC_INFOLIST"
        case .SYNC_OFFLINE_FILES_SIZE:
            return "SYNC_OFFLINE_FILES_SIZE"
        case .SYNC_START:
            return "SYNC_START"
        case .SYNC_STOP:
            return "SYNC_STOP"
        case .SYNC_STATUS:
            return "SYNC_STATUS"
        case .SYNC_ADD:
            return "SYNC_ADD"
        case .SYNC_ADD2:
            return "SYNC_ADD2"
        case .SYNC_START_AFTER_LOGIN:
            return "SYNC_START_AFTER_LOGIN"
        case .SYNC_DELETE:
            return "SYNC_DELETE"
        case .SYNC_GETPUBLICLINKURL:
            return "SYNC_GETPUBLICLINKURL"
        case .SYNC_GETPRIVATELINKURL:
            return "SYNC_GETPRIVATELINKURL"
        case .SYNC_TRIGGER_PROGRESS_UPDATE:
            return "SYNC_TRIGGER_PROGRESS_UPDATE"
        case .SYNC_SETSUPPORTSVIRTUALFILES:
            return "SYNC_SETSUPPORTSVIRTUALFILES"
        case .SYNC_ACKNOWLEDGE_MANY_DELETES:
            return "SYNC_ACKNOWLEDGE_MANY_DELETES"
        case .BLACKLISTED_NODE_LIST:
            return "BLACKLISTED_NODE_LIST"
        case .BLACKLISTED_NODE_SETLIST:
            return "BLACKLISTED_NODE_SETLIST"
        case .NODE_PATH:
            return "NODE_PATH"
        case .NODE_INFO:
            return "NODE_INFO"
        case .NODE_SUBFOLDERS:
            return "NODE_SUBFOLDERS"
        case .NODE_SUBFOLDERS2:
            return "NODE_SUBFOLDERS2"
        case .NODE_FOLDER_SIZE:
            return "NODE_FOLDER_SIZE"
        case .NODE_CREATEMISSINGFOLDERS:
            return "NODE_CREATEMISSINGFOLDERS"
        case .NODE_CREATEMISSINGFOLDERS_LEGACY:
            return "NODE_CREATEMISSINGFOLDERS_LEGACY"
        case .NODE_CONFLICT_INFO:
            return "NODE_CONFLICT_INFO"
        case .ERROR_INFOLIST:
            return "ERROR_INFOLIST"
        case .ERROR_INFOLIST_LEGACY:
            return "ERROR_INFOLIST_LEGACY"
        case .ERROR_GET_CONFLICTS_LEGACY:
            return "ERROR_GET_CONFLICTS_LEGACY"
        case .ERROR_DELETE_SERVER:
            return "ERROR_DELETE_SERVER"
        case .ERROR_DELETE_SYNC:
            return "ERROR_DELETE_SYNC"
        case .ERROR_DELETE_INVALIDTOKEN:
            return "ERROR_DELETE_INVALIDTOKEN"
        case .ERROR_DELETE:
            return "ERROR_DELETE"
        case .ERROR_SYNC_REFRESH:
            return "ERROR_SYNC_REFRESH"
        case .ERROR_RESOLVE_CONFLICTS_LEGACY:
            return "ERROR_RESOLVE_CONFLICTS_LEGACY"
        case .ERROR_RESOLVE_CONFLICTS:
            return "ERROR_RESOLVE_CONFLICTS"
        case .ERROR_RESOLVE_CONFLICTS_QUICK:
            return "ERROR_RESOLVE_CONFLICTS_QUICK"
        case .ERROR_RESOLVE_UNSUPPORTED_CHAR_LEGACY:
            return "ERROR_RESOLVE_UNSUPPORTED_CHAR_LEGACY"
        case .EXCLTEMPL_GETEXCLUDED:
            return "EXCLTEMPL_GETEXCLUDED"
        case .EXCLTEMPL_GETLIST:
            return "EXCLTEMPL_GETLIST"
        case .EXCLTEMPL_SETUSERLIST:
            return "EXCLTEMPL_SETUSERLIST"
        case .EXCLAPP_GETLIST:
            return "EXCLAPP_GETLIST"
        case .EXCLAPP_SETLIST:
            return "EXCLAPP_SETLIST"
        case .EXCLAPP_GET_FETCHING_APP_LIST:
            return "EXCLAPP_GET_FETCHING_APP_LIST"
        case .PARAMETERS_INFO:
            return "PARAMETERS_INFO"
        case .PARAMETERS_UPDATE:
            return "PARAMETERS_UPDATE"
        case .UTILITY_BESTVFSAVAILABLEMODE:
            return "UTILITY_BESTVFSAVAILABLEMODE"
        case .UTILITY_BESTVFSAVAILABLEMODE_LEGACY:
            return "UTILITY_BESTVFSAVAILABLEMODE_LEGACY"
        case .UTILITY_FINDGOODPATHFORNEWSYNC:
            return "UTILITY_FINDGOODPATHFORNEWSYNC"
        case .UTILITY_ISPATHVALIDFORNEWSYNC:
            return "UTILITY_ISPATHVALIDFORNEWSYNC"
        case .UTILITY_ACTIVATELOADINFO:
            return "UTILITY_ACTIVATELOADINFO"
        case .UTILITY_CHECKCOMMSTATUS:
            return "UTILITY_CHECKCOMMSTATUS"
        case .UTILITY_HASSYSTEMLAUNCHONSTARTUP:
            return "UTILITY_HASSYSTEMLAUNCHONSTARTUP"
        case .UTILITY_HASLAUNCHONSTARTUP:
            return "UTILITY_HASLAUNCHONSTARTUP"
        case .UTILITY_SETLAUNCHONSTARTUP:
            return "UTILITY_SETLAUNCHONSTARTUP"
        case .UTILITY_SET_APPSTATE:
            return "UTILITY_SET_APPSTATE"
        case .UTILITY_GET_APPSTATE:
            return "UTILITY_GET_APPSTATE"
        case .UTILITY_SEND_LOG_TO_SUPPORT:
            return "UTILITY_SEND_LOG_TO_SUPPORT"
        case .UTILITY_CANCEL_LOG_TO_SUPPORT:
            return "UTILITY_CANCEL_LOG_TO_SUPPORT"
        case .UTILITY_GET_LOG_ESTIMATED_SIZE_LEGACY:
            return "UTILITY_GET_LOG_ESTIMATED_SIZE_LEGACY"
        case .UTILITY_CRASH:
            return "UTILITY_CRASH"
        case .UTILITY_QUIT:
            return "UTILITY_QUIT"
        case .UTILITY_SEND_APP_START_TRACE:
            return "UTILITY_SEND_APP_START_TRACE"
        case .UTILITY_INSTALL_MAC_LITESYNC_EXT:
            return "UTILITY_INSTALL_MAC_LITESYNC_EXT"
        case .UTILITY_CHECK_MACOS_PERMISSIONS:
            return "UTILITY_CHECK_MACOS_PERMISSIONS"
        case .UPDATER_CHANGE_CHANNEL:
            return "UPDATER_CHANGE_CHANNEL"
        case .UPDATER_VERSION_INFO:
            return "UPDATER_VERSION_INFO"
        case .UPDATER_STATE:
            return "UPDATER_STATE"
        case .UPDATER_START_INSTALLER:
            return "UPDATER_START_INSTALLER"
        case .UPDATER_SKIP_VERSION:
            return "UPDATER_SKIP_VERSION"
        case .EnumEnd:
            return "EnumEnd"
        @unknown default:
            return "RequestNum(rawValue: \(rawValue))"
        }
    }
}
