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

using Infomaniak.kDrive.Types;
using Microsoft.Extensions.Logging;
using static Infomaniak.kDrive.ViewModels.NetworkSettings;

namespace Infomaniak.kDrive.ServerCommunication
{
    public struct CommShared
    {
        public enum RequestNum
        {
            Unknown = 0,
            LoginRequestToken,
            UserDbIds,
            UserInfoList,
            USER_DELETE,
            USER_AVAILABLEDRIVES,
            AccountInfoList,
            DriveInfoList,
            DRIVE_UPDATE,
            DRIVE_DELETE,
            DRIVE_SEARCH,
            SyncInfoList,
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
            PARAMETERS_INFO,
            PARAMETERS_UPDATE,
            UTILITY_FINDGOODPATHFORNEWSYNC,
            UTILITY_BESTVFSAVAILABLEMODE,
            UTILITY_SHOWSHORTCUT,
            UTILITY_SETSHOWSHORTCUT,
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
            EnumEnd
        };
        public enum SignalNum
        {
            Unknown = 0,
            // User
            UserAdded,
            UserUpdated,
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
        }

        public enum NotificationsDisabled
        {
            Never,
            OneHour,
            UntilTomorrow,
            ThreeDays,
            OneWeek,
            Always
        };

        public enum ProxyType
        {
            Undefined,
            None,
            System,
            HTTP
        };
        public enum CommMessageType
        {
            Unknown = 0,
            Request = 1,
            Signal = 2,
        }

        public struct UserInfo
        {
            public DbId? DbId { get; set; }
            public UserId? UserId { get; set; }
            public string? Name { get; set; }
            public string? Email { get; set; }
            public byte[]? Avatar { get; set; }
            public bool? IsConnected { get; set; }
            public bool? IsStaff { get; set; }
        }

        public struct AccountInfo
        {
            public DbId? DbId { get; set; }
            public DbId? UserDbId { get; set; }
        }

        public struct DriveInfo
        {
            public DbId? DbId { get; set; }
            public DbId? AccountDbId { get; set; }
            public DriveId? DriveId { get; set; }
            public string? Name { get; set; }
            public System.Drawing.Color? Color { get; set; }
            public bool? Notification { get; set; }
            public bool? Maintenance { get; set; }
            public bool? Locked { get; set; }
            public bool? AccessDenied { get; set; }
        }

        public struct SyncInfo
        {
            public DbId? DbId { get; set; }
            public DbId? DriveDbId { get; set; }
            public string? LocalPath { get; set; }
            public string? TargetPath { get; set; }
            public string? TargetNodeId { get; set; }
            public bool? SupportOnlineMode { get; set; }
            public bool? OnlineMode { get; set; }
        }

        public struct ProxyConfigInfo
        {
            public ProxyType? Type;
            public string? HostName;
            public int? Port;
            public bool? NeedsAuth;
            public string? User;
            public string? Pwd;
        }

        public struct ParmsInfo
        {
            public Language? Language;
            public bool? MonoIcons;
            public bool? AutoStart;
            public bool? MoveToTrash;
            public NotificationsDisabled? NotificationsDisabled;
            public bool? UseLog;
            public LogLevel? LogLevel;
            public bool? ExtendedLog;
            public bool? PurgeOldLogs;
            public ProxyConfigInfo? ProxyConfigInfo;
            public bool? UseBigFolderSizeLimit;
            public long? BigFolderSizeLimit;
            public bool? ShowShortcuts;
            public VersionChannel? DistributionChannel;
        }
    }
}
