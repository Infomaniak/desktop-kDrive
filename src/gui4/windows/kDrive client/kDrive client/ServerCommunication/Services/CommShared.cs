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

using Infomaniak.kDrive.ViewModels;
using static System.Windows.Forms.VisualStyles.VisualStyleElement.ListView;

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
            AccountInfoList,
            DriveInfoList,
            SyncInfoList
        }

        public enum SignalNum
        {
            Unknown = 0,
            UserAdded,
            UserUpdated,
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
            public int? Color { get; set; }
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
    }
}
