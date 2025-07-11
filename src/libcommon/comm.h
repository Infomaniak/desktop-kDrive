/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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

#pragma once
#include <string>

#define COMM_SHORT_TIMEOUT 1000
#define COMM_AVERAGE_TIMEOUT 10000
#define COMM_LONG_TIMEOUT 60000

#define MSG_TYPE "type"

#define MSG_REQUEST_ID "id"
#define MSG_REQUEST_NUM "num"
#define MSG_REQUEST_PARAMS "params"

#define MSG_REPLY_ID "id"
#define MSG_REPLY_RESULT "result"

#define MSG_SIGNAL_ID "id"
#define MSG_SIGNAL_NUM "num"
#define MSG_SIGNAL_PARAMS "params"

#define EXECUTE_ERROR_MSG "C/S function call timeout or error!"

enum class MsgType {
    REQUEST = 0,
    REPLY,
    SIGNAL
};

enum class RequestNum {
    LOGIN_REQUESTTOKEN = 1,
    USER_DBIDLIST,
    USER_INFOLIST,
    USER_DELETE,
    USER_AVAILABLEDRIVES,
    USER_ID_FROM_USERDBID,
    ACCOUNT_INFOLIST,
    DRIVE_INFOLIST,
    DRIVE_INFO,
    DRIVE_ID_FROM_DRIVEDBID,
    DRIVE_ID_FROM_SYNCDBID,
    DRIVE_DEFAULTCOLOR,
    DRIVE_UPDATE,
    DRIVE_DELETE,
    SYNC_INFOLIST,
    SYNC_START,
    SYNC_STOP,
    SYNC_STATUS,
    SYNC_ISRUNNING,
    SYNC_ADD,
    SYNC_ADD2,
    SYNC_START_AFTER_LOGIN,
    SYNC_DELETE,
    SYNC_GETPUBLICLINKURL,
    SYNC_GETPRIVATELINKURL,
    SYNC_ASKFORSTATUS,
    SYNC_SETSUPPORTSVIRTUALFILES,
    SYNC_SETROOTPINSTATE,
    SYNC_PROPAGATE_SYNCLIST_CHANGE,
    SYNCNODE_LIST,
    SYNCNODE_SETLIST,
    NODE_PATH,
    NODE_INFO,
    NODE_SUBFOLDERS,
    NODE_SUBFOLDERS2,
    NODE_FOLDER_SIZE,
    NODE_CREATEMISSINGFOLDERS,
    ERROR_INFOLIST,
    ERROR_GET_CONFLICTS,
    ERROR_DELETE_SERVER,
    ERROR_DELETE_SYNC,
    ERROR_DELETE_INVALIDTOKEN,
    ERROR_RESOLVE_CONFLICTS,
    ERROR_RESOLVE_UNSUPPORTED_CHAR,
    EXCLTEMPL_GETEXCLUDED,
    EXCLTEMPL_GETLIST,
    EXCLTEMPL_SETLIST,
    EXCLTEMPL_PROPAGATE_CHANGE,
#if defined(KD_MACOS)
    EXCLAPP_GETLIST,
    EXCLAPP_SETLIST,
    EXCLAPP_GET_FETCHING_APP_LIST,
#endif
    PARAMETERS_INFO,
    PARAMETERS_UPDATE,
    UTILITY_FINDGOODPATHFORNEWSYNC,
    UTILITY_BESTVFSAVAILABLEMODE,
#if defined(KD_WINDOWS)
    UTILITY_SHOWSHORTCUT,
    UTILITY_SETSHOWSHORTCUT,
#endif
    UTILITY_ACTIVATELOADINFO,
    UTILITY_CHECKCOMMSTATUS,
    UTILITY_HASSYSTEMLAUNCHONSTARTUP,
    UTILITY_HASLAUNCHONSTARTUP,
    UTILITY_SETLAUNCHONSTARTUP,
    UTILITY_SET_APPSTATE,
    UTILITY_GET_APPSTATE,
    UTILITY_SEND_LOG_TO_SUPPORT,
    UTILITY_CANCEL_LOG_TO_SUPPORT,
    UTILITY_GET_LOG_ESTIMATED_SIZE,
    UTILITY_CRASH,
    UTILITY_QUIT,
    UTILITY_DISPLAY_CLIENT_REPORT, // Sent by the Client process as soon the UI is visible for the user.
    UPDATER_CHANGE_CHANNEL,
    UPDATER_VERSION_INFO,
    UPDATER_STATE,
    UPDATER_START_INSTALLER,
    UPDATER_SKIP_VERSION,
};

inline std::string toString(RequestNum e) {
    switch (e) {
        case RequestNum::LOGIN_REQUESTTOKEN:
            return "LOGIN_REQUESTTOKEN";
        case RequestNum::USER_DBIDLIST:
            return "USER_DBIDLIST";
        case RequestNum::USER_INFOLIST:
            return "USER_INFOLIST";
        case RequestNum::USER_DELETE:
            return "USER_DELETE";
        case RequestNum::USER_AVAILABLEDRIVES:
            return "USER_AVAILABLEDRIVES";
        case RequestNum::USER_ID_FROM_USERDBID:
            return "USER_ID_FROM_USERDBID";
        case RequestNum::ACCOUNT_INFOLIST:
            return "ACCOUNT_INFOLIST";
        case RequestNum::DRIVE_INFOLIST:
            return "DRIVE_INFOLIST";
        case RequestNum::DRIVE_INFO:
            return "DRIVE_INFO";
        case RequestNum::DRIVE_ID_FROM_DRIVEDBID:
            return "DRIVE_ID_FROM_DRIVEDBID";
        case RequestNum::DRIVE_ID_FROM_SYNCDBID:
            return "DRIVE_ID_FROM_SYNCDBID";
        case RequestNum::DRIVE_DEFAULTCOLOR:
            return "DRIVE_DEFAULTCOLOR";
        case RequestNum::DRIVE_UPDATE:
            return "DRIVE_UPDATE";
        case RequestNum::DRIVE_DELETE:
            return "DRIVE_DELETE";
        case RequestNum::SYNC_INFOLIST:
            return "SYNC_INFOLIST";
        case RequestNum::SYNC_START:
            return "SYNC_START";
        case RequestNum::SYNC_STOP:
            return "SYNC_STOP";
        case RequestNum::SYNC_STATUS:
            return "SYNC_STATUS";
        case RequestNum::SYNC_ISRUNNING:
            return "SYNC_ISRUNNING";
        case RequestNum::SYNC_ADD:
            return "SYNC_ADD";
        case RequestNum::SYNC_ADD2:
            return "SYNC_ADD2";
        case RequestNum::SYNC_START_AFTER_LOGIN:
            return "SYNC_START_AFTER_LOGIN";
        case RequestNum::SYNC_DELETE:
            return "SYNC_DELETE";
        case RequestNum::SYNC_GETPUBLICLINKURL:
            return "SYNC_GETPUBLICLINKURL";
        case RequestNum::SYNC_GETPRIVATELINKURL:
            return "SYNC_GETPRIVATELINKURL";
        case RequestNum::SYNC_ASKFORSTATUS:
            return "SYNC_ASKFORSTATUS";
        case RequestNum::SYNC_SETSUPPORTSVIRTUALFILES:
            return "SYNC_SETSUPPORTSVIRTUALFILES";
        case RequestNum::SYNC_SETROOTPINSTATE:
            return "SYNC_SETROOTPINSTATE";
        case RequestNum::SYNC_PROPAGATE_SYNCLIST_CHANGE:
            return "SYNC_PROPAGATE_SYNCLIST_CHANGE";
        case RequestNum::SYNCNODE_LIST:
            return "SYNCNODE_LIST";
        case RequestNum::SYNCNODE_SETLIST:
            return "SYNCNODE_SETLIST";
        case RequestNum::NODE_PATH:
            return "NODE_PATH";
        case RequestNum::NODE_INFO:
            return "NODE_INFO";
        case RequestNum::NODE_SUBFOLDERS:
            return "NODE_SUBFOLDERS";
        case RequestNum::NODE_SUBFOLDERS2:
            return "NODE_SUBFOLDERS2";
        case RequestNum::NODE_FOLDER_SIZE:
            return "NODE_FOLDER_SIZE";
        case RequestNum::NODE_CREATEMISSINGFOLDERS:
            return "NODE_CREATEMISSINGFOLDERS";
        case RequestNum::ERROR_INFOLIST:
            return "ERROR_INFOLIST";
        case RequestNum::ERROR_GET_CONFLICTS:
            return "ERROR_GET_CONFLICTS";
        case RequestNum::ERROR_DELETE_SERVER:
            return "ERROR_DELETE_SERVER";
        case RequestNum::ERROR_DELETE_SYNC:
            return "ERROR_DELETE_SYNC";
        case RequestNum::ERROR_DELETE_INVALIDTOKEN:
            return "ERROR_DELETE_INVALIDTOKEN";
        case RequestNum::ERROR_RESOLVE_CONFLICTS:
            return "ERROR_RESOLVE_CONFLICTS";
        case RequestNum::ERROR_RESOLVE_UNSUPPORTED_CHAR:
            return "ERROR_RESOLVE_UNSUPPORTED_CHAR";
        case RequestNum::EXCLTEMPL_GETEXCLUDED:
            return "EXCLTEMPL_GETEXCLUDED";
        case RequestNum::EXCLTEMPL_GETLIST:
            return "EXCLTEMPL_GETLIST";
        case RequestNum::EXCLTEMPL_SETLIST:
            return "EXCLTEMPL_SETLIST";
        case RequestNum::EXCLTEMPL_PROPAGATE_CHANGE:
            return "EXCLTEMPL_PROPAGATE_CHANGE";
#if defined(KD_MACOS)
        case RequestNum::EXCLAPP_GETLIST:
            return "EXCLAPP_GETLIST";
        case RequestNum::EXCLAPP_SETLIST:
            return "EXCLAPP_SETLIST";
        case RequestNum::EXCLAPP_GET_FETCHING_APP_LIST:
            return "GET_FETCHING_APP_LIST";
#endif
        case RequestNum::PARAMETERS_INFO:
            return "PARAMETERS_INFO";
        case RequestNum::PARAMETERS_UPDATE:
            return "PARAMETERS_UPDATE";
        case RequestNum::UTILITY_FINDGOODPATHFORNEWSYNC:
            return "UTILITY_FINDGOODPATHFORNEWSYNC";
        case RequestNum::UTILITY_BESTVFSAVAILABLEMODE:
            return "UTILITY_BESTVFSAVAILABLEMODE";
#if defined(KD_WINDOWS)
        case RequestNum::UTILITY_SHOWSHORTCUT:
            return "UTILITY_SHOWSHORTCUT";
        case RequestNum::UTILITY_SETSHOWSHORTCUT:
            return "UTILITY_SETSHOWSHORTCUT";
#endif
        case RequestNum::UTILITY_ACTIVATELOADINFO:
            return "UTILITY_ACTIVATELOADINFO";
        case RequestNum::UTILITY_CHECKCOMMSTATUS:
            return "UTILITY_CHECKCOMMSTATUS";
        case RequestNum::UTILITY_HASSYSTEMLAUNCHONSTARTUP:
            return "UTILITY_HASSYSTEMLAUNCHONSTARTUP";
        case RequestNum::UTILITY_HASLAUNCHONSTARTUP:
            return "UTILITY_HASLAUNCHONSTARTUP";
        case RequestNum::UTILITY_SETLAUNCHONSTARTUP:
            return "UTILITY_SETLAUNCHONSTARTUP";
        case RequestNum::UTILITY_SET_APPSTATE:
            return "UTILITY_SET_APPSTATE";
        case RequestNum::UTILITY_GET_APPSTATE:
            return "UTILITY_GET_APPSTATE";
        case RequestNum::UTILITY_SEND_LOG_TO_SUPPORT:
            return "UTILITY_SEND_LOG_TO_SUPPORT";
        case RequestNum::UTILITY_CANCEL_LOG_TO_SUPPORT:
            return "UTILITY_CANCEL_LOG_TO_SUPPORT";
        case RequestNum::UTILITY_GET_LOG_ESTIMATED_SIZE:
            return "UTILITY_GET_LOG_ESTIMATED_SIZE";
        case RequestNum::UTILITY_CRASH:
            return "UTILITY_CRASH";
        case RequestNum::UTILITY_QUIT:
            return "UTILITY_QUIT";
        case RequestNum::UTILITY_DISPLAY_CLIENT_REPORT:
            return "UTILITY_DISPLAY_CLIENT_REPORT";
        case RequestNum::UPDATER_VERSION_INFO:
            return "UPDATER_VERSION_INFO";
        case RequestNum::UPDATER_STATE:
            return "UPDATER_STATE";
        case RequestNum::UPDATER_START_INSTALLER:
            return "UPDATER_START_INSTALLER";
        case RequestNum::UPDATER_SKIP_VERSION:
            return "UPDATER_SKIP_VERSION";
        default:
            return "No conversion to string available";
    }
}

enum class SignalNum {
    // User
    USER_ADDED = 0,
    USER_UPDATED,
    USER_STATUSCHANGED,
    USER_REMOVED,
    // Account
    ACCOUNT_ADDED,
    ACCOUNT_UPDATED,
    ACCOUNT_REMOVED,
    // Drive
    DRIVE_ADDED,
    DRIVE_UPDATED,
    DRIVE_QUOTAUPDATED,
    DRIVE_REMOVED,
    DRIVE_DELETE_FAILED,
    // Sync
    SYNC_ADDED,
    SYNC_UPDATED,
    SYNC_REMOVED,
    SYNC_PROGRESSINFO,
    SYNC_COMPLETEDITEM,
    SYNC_VFS_CONVERSION_COMPLETED,
    SYNC_DELETE_FAILED,
    // Node
    NODE_FOLDER_SIZE_COMPLETED,
    NODE_FIX_CONFLICTED_FILES_COMPLETED,
    // Updater
    UPDATER_SHOW_DIALOG,
    UPDATER_STATE_CHANGED,
    // Utility
    UTILITY_SHOW_NOTIFICATION,
    UTILITY_NEW_BIG_FOLDER,
    UTILITY_ERROR_ADDED,
    UTILITY_ERRORS_CLEARED,
    UTILITY_SHOW_SETTINGS,
    UTILITY_SHOW_SYNTHESIS,
    UTILITY_LOG_UPLOAD_STATUS_UPDATED,
    UTILITY_QUIT
};

inline std::string toString(SignalNum e) {
    switch (e) {
        case SignalNum::USER_ADDED:
            return "USER_ADDED";
        case SignalNum::USER_UPDATED:
            return "USER_UPDATED";
        case SignalNum::USER_STATUSCHANGED:
            return "USER_STATUSCHANGED";
        case SignalNum::USER_REMOVED:
            return "USER_REMOVED";
        case SignalNum::ACCOUNT_ADDED:
            return "ACCOUNT_ADDED";
        case SignalNum::ACCOUNT_UPDATED:
            return "ACCOUNT_UPDATED";
        case SignalNum::ACCOUNT_REMOVED:
            return "ACCOUNT_REMOVED";
        case SignalNum::DRIVE_ADDED:
            return "DRIVE_ADDED";
        case SignalNum::DRIVE_UPDATED:
            return "DRIVE_UPDATED";
        case SignalNum::DRIVE_QUOTAUPDATED:
            return "DRIVE_QUOTAUPDATED";
        case SignalNum::DRIVE_REMOVED:
            return "DRIVE_REMOVED";
        case SignalNum::DRIVE_DELETE_FAILED:
            return "DRIVE_DELETE_FAILED";
        case SignalNum::SYNC_ADDED:
            return "SYNC_ADDED";
        case SignalNum::SYNC_UPDATED:
            return "SYNC_UPDATED";
        case SignalNum::SYNC_REMOVED:
            return "SYNC_REMOVED";
        case SignalNum::SYNC_PROGRESSINFO:
            return "SYNC_PROGRESSINFO";
        case SignalNum::SYNC_COMPLETEDITEM:
            return "SYNC_COMPLETEDITEM";
        case SignalNum::SYNC_VFS_CONVERSION_COMPLETED:
            return "SYNC_VFS_CONVERSION_COMPLETED";
        case SignalNum::SYNC_DELETE_FAILED:
            return "SYNC_DELETE_FAILED";
        case SignalNum::NODE_FOLDER_SIZE_COMPLETED:
            return "NODE_FOLDER_SIZE_COMPLETED";
        case SignalNum::NODE_FIX_CONFLICTED_FILES_COMPLETED:
            return "NODE_FIX_CONFLICTED_FILES_COMPLETED";
        case SignalNum::UPDATER_SHOW_DIALOG:
            return "UPDATER_SHOW_DIALOG";
        case SignalNum::UPDATER_STATE_CHANGED:
            return "UPDATER_STATE_CHANGED";
        case SignalNum::UTILITY_SHOW_NOTIFICATION:
            return "UTILITY_SHOW_NOTIFICATION";
        case SignalNum::UTILITY_NEW_BIG_FOLDER:
            return "UTILITY_NEW_BIG_FOLDER";
        case SignalNum::UTILITY_ERROR_ADDED:
            return "UTILITY_ERROR_ADDED";
        case SignalNum::UTILITY_ERRORS_CLEARED:
            return "UTILITY_ERRORS_CLEARED";
        case SignalNum::UTILITY_SHOW_SETTINGS:
            return "UTILITY_SHOW_SETTINGS";
        case SignalNum::UTILITY_SHOW_SYNTHESIS:
            return "UTILITY_SHOW_SYNTHESIS";
        case SignalNum::UTILITY_LOG_UPLOAD_STATUS_UPDATED:
            return "UTILITY_LOG_UPLOAD_STATUS_UPDATED";
        case SignalNum::UTILITY_QUIT:
            return "UTILITY_QUIT";
        default:
            return "No conversion to string available";
    }
}
