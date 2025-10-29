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
using Infomaniak.kDrive.ViewModels;

// These structs are used for communication between the client and server
// they should only be used in the ServerCommunication layer
namespace Infomaniak.kDrive.ServerCommunication.CommStruct
{
    public static partial class ConversionHelper
    {
        static void copyProperty(object source, object target, string sourcepropertyName, string targetpropertyName)
        {
            var sourceProp = source.GetType().GetProperty(sourcepropertyName);
            var targetProp = target.GetType().GetProperty(targetpropertyName);
            if (sourceProp is not null && targetProp is not null)
            {
                var value = sourceProp.GetValue(source);
                if (value is not null)
                    targetProp.SetValue(target, value);
            }
            else
            {
                Logger.Log(Logger.Level.Error, $"Property not found. Source property: {sourcepropertyName}, Target property: {targetpropertyName} (Source type: {source.GetType().FullName}, Target type: {target.GetType().FullName})");
            }
        }
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

    public static partial class ConversionHelper
    {
        static public void copyToUser(UserInfo source, User target)
        {
            copyProperty(source, target, nameof(source.DbId), nameof(target.DbId));
            copyProperty(source, target, nameof(source.UserId), nameof(target.UserId));
            copyProperty(source, target, nameof(source.Name), nameof(target.Name));
            copyProperty(source, target, nameof(source.Email), nameof(target.Email));
            copyProperty(source, target, nameof(source.Avatar), nameof(target.Avatar));
            copyProperty(source, target, nameof(source.IsConnected), nameof(target.IsConnected));
            copyProperty(source, target, nameof(source.IsStaff), nameof(target.IsStaff));
        }
    }


    public struct AccountInfo
    {
        public DbId? DbId { get; set; }
        public DbId? UserDbId { get; set; }
    }

    public static partial class ConversionHelper
    {
        static public void copyToAccount(AccountInfo source, Account target)
        {
            copyProperty(source, target, nameof(source.DbId), nameof(target.DbId));
        }
    }


    public struct DriveInfo
    {
        public DbId? DbId { get; set; }
        public DbId? AccountDbId { get; set; }
        public DriveId? DriveId { get; set; }
        public string? Name { get; set; }
        public System.Drawing.Color? Color { get; set; }
    }
    public static partial class ConversionHelper
    {
        static public void copyToDrive(DriveInfo source, Drive target)
        {
            copyProperty(source, target, nameof(source.DbId), nameof(target.DbId));
            copyProperty(source, target, nameof(source.DriveId), nameof(target.DriveId));
            copyProperty(source, target, nameof(source.Name), nameof(target.Name));
            copyProperty(source, target, nameof(source.Color), nameof(target.Color));
        }
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
    public static partial class ConversionHelper
    {
        static public void copyToSync(SyncInfo source, Sync target)
        {
            copyProperty(source, target, nameof(source.DbId), nameof(target.DbId));
            copyProperty(source, target, nameof(source.TargetPath), nameof(target.RemotePath));
            copyProperty(source, target, nameof(source.LocalPath), nameof(target.LocalPath));
        }
    }

    public struct ProxyConfigInfo
    {
        public ProxyType? Type { get; set; }
        public string? HostName { get; set; }
        public int? Port { get; set; }
        public bool? NeedsAuth { get; set; }
        public string? User { get; set; }
        public string? Pwd { get; set; }
    }
    public static partial class ConversionHelper
    {
        static public void copyToProxyConfig(ProxyConfigInfo source, ProxyConfig target)
        {
            copyProperty(source, target, nameof(source.Type), nameof(target.Type));
            copyProperty(source, target, nameof(source.HostName), nameof(target.HostName));
            copyProperty(source, target, nameof(source.Port), nameof(target.Port));
            copyProperty(source, target, nameof(source.NeedsAuth), nameof(target.NeedsAuth));
            copyProperty(source, target, nameof(source.User), nameof(target.User));
            copyProperty(source, target, nameof(source.Pwd), nameof(target.Pwd));
        }
    }

    public struct ParmsInfo
    {
        public Language? Language { get; set; }
        public bool? MonoIcons { get; set; }
        public bool? AutoStart { get; set; }
        public bool? MoveToTrash { get; set; }
        public NotificationsDisabled? NotificationsDisabled { get; set; }
        public bool? UseLog { get; set; }
        public Logger.Level? LogLevel { get; set; }
        public bool? ExtendedLog { get; set; }
        public bool? PurgeOldLogs { get; set; }
        public ProxyConfigInfo? ProxyConfigInfo { get; set; }
        public bool? UseBigFolderSizeLimit { get; set; } // Not implemented
        public long? BigFolderSizeLimit { get; set; } // Not implemented
        public bool? ShowShortcuts { get; set; }
        public VersionChannel? DistributionChannel { get; set; }
    }
    public static partial class ConversionHelper
    {
        static public void copyToSettings(ParmsInfo source, Settings target)
        {
            copyProperty(source, target, nameof(source.Language), nameof(target.Language));
            // copyProperty(source, target, nameof(source.MonoIcons), nameof(target.MonoIcons)); -> Not implemented
            copyProperty(source, target, nameof(source.AutoStart), nameof(target.AutoStart));
            copyProperty(source, target, nameof(source.MoveToTrash), nameof(target.MoveToTrash));
            copyProperty(source, target, nameof(source.NotificationsDisabled), nameof(target.NotificationsDisabled));
            copyProperty(source, target, nameof(source.ShowShortcuts), nameof(target.ShowShortcuts));
            copyProperty(source, target, nameof(source.DistributionChannel), nameof(target.UpdateManager.CurrentChannel));
            copyProperty(source, target, nameof(source.PurgeOldLogs), nameof(target.PurgeOldLogs));

            if (source.ProxyConfigInfo is ProxyConfigInfo proxyConfigInfo)
                copyToProxyConfig(proxyConfigInfo, target.ProxyConfig);

            if (source.UseLog is null)
                source.UseLog = target.LogLevel != Logger.Level.None;

            if (source.UseLog == false)
                target.LogLevel = Logger.Level.None;
            else if (source.ExtendedLog == true)
                target.LogLevel = Logger.Level.Extended;
            else
                copyProperty(source, target, nameof(source.LogLevel), nameof(target.LogLevel));
        }
    }
}
