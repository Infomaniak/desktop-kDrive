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
using System;

// These structs are used for communication between the client and server
// they should only be used in the ServerCommunication layer
namespace Infomaniak.kDrive.ServerCommunication.CommStruct
{
    public static partial class ConversionHelper
    {
        static void CopyProperty(object source, object target, string sourcepropertyName, string targetpropertyName)
        {
            var sourceProp = source.GetType().GetProperty(sourcepropertyName);
            var targetProp = target.GetType().GetProperty(targetpropertyName);
            if (sourceProp is not null && targetProp is not null)
            {
                var sourceValue = sourceProp.GetValue(source);
                if (sourceValue is null)
                    return;

                var targetValue = targetProp.GetValue(target);
                if (targetValue is null)
                {
                    var targetPropType = targetProp.PropertyType;
                    if (targetPropType.IsGenericType && targetPropType.GetGenericTypeDefinition() == typeof(Nullable<>))
                    {
                        targetPropType = Nullable.GetUnderlyingType(targetPropType)!;
                    }
                }

                targetProp.SetValue(target, sourceValue);
            }
            else
            {
                Logger.Log(Logger.Level.Error, $"Property not found. Source property: {sourcepropertyName}, Target property: {targetpropertyName} (Source type: {source.GetType().FullName}, Target type: {target.GetType().FullName})");
            }
        }
    }

    public class UserInfo
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
        public static void CopyToUser(UserInfo source, User target)
        {
            CopyProperty(source, target, nameof(source.DbId), nameof(target.DbId));
            CopyProperty(source, target, nameof(source.UserId), nameof(target.UserId));
            CopyProperty(source, target, nameof(source.Name), nameof(target.Name));
            CopyProperty(source, target, nameof(source.Email), nameof(target.Email));
            CopyProperty(source, target, nameof(source.IsConnected), nameof(target.IsConnected));
            CopyProperty(source, target, nameof(source.IsStaff), nameof(target.IsStaff));

            if (source.Avatar is null)
            {
                target.Avatar = null;
                return;
            }

            if (target.Avatar is null)
            {
                target.Avatar = new byte[source.Avatar.Length];
            }
            Array.Copy(source.Avatar, target.Avatar, source.Avatar.Length);

        }
    }


    public class AccountInfo
    {
        public DbId? DbId { get; set; }
        public DbId? UserDbId { get; set; }
        public AccountId? Id { get; set; }
        public string? Name { get; set; }
    }

    public static partial class ConversionHelper
    {
        public static void CopyToAccount(AccountInfo source, Account target)
        {
            CopyProperty(source, target, nameof(source.DbId), nameof(target.DbId));
            CopyProperty(source, target, nameof(source.Id), nameof(target.Id));
            CopyProperty(source, target, nameof(source.Name), nameof(target.Name));
        }
    }

    public class DriveInfo
    {
        private System.Drawing.Color? _color;
        public DbId? DbId { get; set; }
        public DbId? AccountDbId { get; set; }
        public DriveId? Id { get; set; }
        public string? Name { get; set; }
        public bool? IsFree { get; set; }
        public System.Drawing.Color? Color
        {
            get => _color;
            set
            {
                _color = value;
                Logger.Log(Logger.Level.Info, $"DriveInfo color for {Name} is {_color}");
            }
        }

        public bool? Admin { get; set; }

        public System.Int64 Size { get; set; }
        public System.Int64 UsedSize { get; set; }
    }
    public static partial class ConversionHelper
    {
        public static void CopyToDrive(DriveInfo source, Drive target)
        {
            CopyProperty(source, target, nameof(source.DbId), nameof(target.DbId));
            CopyProperty(source, target, nameof(source.Id), nameof(target.DriveId));
            CopyProperty(source, target, nameof(source.Name), nameof(target.Name));
            CopyProperty(source, target, nameof(source.Color), nameof(target.Color));
            CopyProperty(source, target, nameof(source.Admin), nameof(target.IsAdmin));
            CopyProperty(source, target, nameof(source.IsFree), nameof(target.IsFreeOffer));
            CopyProperty(source, target, nameof(source.Size), nameof(target.Size));
            CopyProperty(source, target, nameof(source.UsedSize), nameof(target.UsedSize));
        }
    }

    public class DriveAvailableInfo
    {
        public DriveId? DriveId { get; set; }
        public UserId? UserId { get; set; }
        public DbId? UserDbId { get; set; }
        public AccountId? AccountId { get; set; }
        public string? AccountName { get; set; }
        public string? Name { get; set; }
        public System.Drawing.Color? Color { get; set; }
    }
    public static partial class ConversionHelper
    {
        public static void CopyToDriveAvailable(DriveAvailableInfo source, DriveAvailable target)
        {
            CopyProperty(source, target, nameof(source.DriveId), nameof(target.DriveId));
            CopyProperty(source, target, nameof(source.UserId), nameof(target.UserId));
            CopyProperty(source, target, nameof(source.AccountId), nameof(target.AccountId));
            CopyProperty(source, target, nameof(source.AccountName), nameof(target.AccountName));
            CopyProperty(source, target, nameof(source.Name), nameof(target.Name));
            CopyProperty(source, target, nameof(source.Color), nameof(target.Color));
            CopyProperty(source, target, nameof(source.UserDbId), nameof(target.UserDbId));
        }
    }

    public class SyncInfo
    {
        public DbId? DbId { get; set; }
        public DbId? DriveDbId { get; set; }
        public string? LocalPath { get; set; }
        public string? TargetPath { get; set; }
        public string? TargetNodeId { get; set; }
        public bool? SupportVfs { get; set; }
        public VirtualFileMode? VirtualFileMode { get; set; }
    }
    public static partial class ConversionHelper
    {
        public static void CopyToSync(SyncInfo source, Sync target)
        {
            CopyProperty(source, target, nameof(source.DbId), nameof(target.DbId));
            CopyProperty(source, target, nameof(source.TargetPath), nameof(target.RemotePath));
            CopyProperty(source, target, nameof(source.TargetNodeId), nameof(target.RemoteNodeId));
            CopyProperty(source, target, nameof(source.LocalPath), nameof(target.LocalPath));
            CopyProperty(source, target, nameof(source.SupportVfs), nameof(target.SupportOnlineMode));

            if (source.VirtualFileMode is not null)
                if (source.VirtualFileMode.Value == VirtualFileMode.Win)
                    target.SyncType = SyncType.Online;
                else
                    target.SyncType = SyncType.Offline;
        }
    }

    public class ProxyConfigInfo
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
        public static void CopyToProxyConfig(ProxyConfigInfo source, ProxyConfig target)
        {
            CopyProperty(source, target, nameof(source.Type), nameof(target.Type));
            CopyProperty(source, target, nameof(source.HostName), nameof(target.HostName));
            CopyProperty(source, target, nameof(source.Port), nameof(target.Port));
            CopyProperty(source, target, nameof(source.NeedsAuth), nameof(target.NeedsAuth));
            CopyProperty(source, target, nameof(source.User), nameof(target.User));
            CopyProperty(source, target, nameof(source.Pwd), nameof(target.Pwd));
        }
        public static void CopyToProxyConfigInfo(ProxyConfig source, ProxyConfigInfo target)
        {
            CopyProperty(source, target, nameof(source.Type), nameof(target.Type));
            CopyProperty(source, target, nameof(source.HostName), nameof(target.HostName));
            CopyProperty(source, target, nameof(source.Port), nameof(target.Port));
            CopyProperty(source, target, nameof(source.NeedsAuth), nameof(target.NeedsAuth));
            CopyProperty(source, target, nameof(source.User), nameof(target.User));
            CopyProperty(source, target, nameof(source.Pwd), nameof(target.Pwd));
        }
    }

    public class ParmsInfo
    {
        public Language? Language { get; set; }
        public bool? AutoStart { get; set; }
        public bool? MoveToTrash { get; set; }
        public NotificationsDisabled? NotificationsDisabled { get; set; }
        public bool? UseLog { get; set; }
        public Logger.Level? LogLevel { get; set; }
        public bool? ExtendedLog { get; set; }
        public bool? PurgeOldLogs { get; set; }
        public ProxyConfigInfo? ProxyConfigInfo { get; set; }
        public bool? MatomoEnabled { get; set; }
        public bool? SentryEnabled { get; set; }
        public VersionChannel? DistributionChannel { get; set; }
    }
    public static partial class ConversionHelper
    {
        public static void CopyToSettings(ParmsInfo source, Settings target)
        {
            CopyProperty(source, target, nameof(source.Language), nameof(target.Language));
            CopyProperty(source, target, nameof(source.AutoStart), nameof(target.AutoStart));
            CopyProperty(source, target, nameof(source.MoveToTrash), nameof(target.MoveToTrash));
            CopyProperty(source, target, nameof(source.NotificationsDisabled), nameof(target.NotificationsDisabled));
            CopyProperty(source, target.UpdateManager, nameof(source.DistributionChannel), nameof(target.UpdateManager.CurrentChannel));
            CopyProperty(source, target, nameof(source.PurgeOldLogs), nameof(target.PurgeOldLogs));
            CopyProperty(source, target, nameof(source.MatomoEnabled), nameof(target.MatomoEnabled));
            CopyProperty(source, target, nameof(source.SentryEnabled), nameof(target.SentryEnabled));

            if (source.ProxyConfigInfo is ProxyConfigInfo proxyConfigInfo)
                CopyToProxyConfig(proxyConfigInfo, target.ProxyConfig);

            if (source.UseLog is null)
                source.UseLog = target.LogLevel != Logger.Level.None;

            if (source.UseLog == false)
                target.LogLevel = Logger.Level.None;
            else if (source.ExtendedLog == true)
                target.LogLevel = Logger.Level.Extended;
            else
                CopyProperty(source, target, nameof(source.LogLevel), nameof(target.LogLevel));
        }
        public static void CopyToParmsInfo(Settings source, ParmsInfo target)
        {
            CopyProperty(source, target, nameof(source.Language), nameof(target.Language));
            CopyProperty(source, target, nameof(source.AutoStart), nameof(target.AutoStart));
            CopyProperty(source, target, nameof(source.MoveToTrash), nameof(target.MoveToTrash));
            CopyProperty(source, target, nameof(source.NotificationsDisabled), nameof(target.NotificationsDisabled));
            CopyProperty(source.UpdateManager, target, nameof(source.UpdateManager.CurrentChannel), nameof(target.DistributionChannel));
            CopyProperty(source, target, nameof(source.PurgeOldLogs), nameof(target.PurgeOldLogs));
            CopyProperty(source, target, nameof(source.MatomoEnabled), nameof(target.MatomoEnabled));
            CopyProperty(source, target, nameof(source.SentryEnabled), nameof(target.SentryEnabled));

            if (target.ProxyConfigInfo is null)
                target.ProxyConfigInfo = new();

            CopyToProxyConfigInfo(source.ProxyConfig, target.ProxyConfigInfo);

            if (source.LogLevel == Logger.Level.None)
            {
                target.UseLog = false;
                target.ExtendedLog = false;
                target.LogLevel = Logger.Level.Info;
            }
            else if (source.LogLevel == Logger.Level.Extended)
            {
                target.UseLog = true;
                target.ExtendedLog = true;
                target.LogLevel = Logger.Level.Info;
            }
            else
            {
                target.UseLog = true;
                target.ExtendedLog = false;
                target.LogLevel = source.LogLevel;
            }
        }
    }

    public class SyncFileItemInfo
    {
        public NodeType? Type { get; set; }
        public string? Path { get; set; }
        public string? NewPath { get; set; }
        public NodeId? LocalNodeId { get; set; }
        public NodeId? RemoteNodeId { get; set; }

        public SyncDirection? Direction { get; set; }
        public SyncFileInstruction? Instruction { get; set; }
        public SyncFileStatus? Status { get; set; }
        public ConflictType? Conflict { get; set; }
        public InconsistencyType? Inconsistency { get; set; }
        public CancelType? CancelType { get; set; }
        public string? Error { get; set; }
        public Int64? Size { get; set; }
        public int? Progress { get; set; }
        public Int64? OperationId { get; set; }
    }

    public static partial class ConversionHelper
    {
        public static void CopyToSyncFileItem(SyncFileItemInfo source, SyncFileItem target)
        {
            CopyProperty(source, target, nameof(source.Type), nameof(target.Type));
            CopyProperty(source, target, nameof(source.Path), nameof(target.Path));
            CopyProperty(source, target, nameof(source.NewPath), nameof(target.NewPath));
            CopyProperty(source, target, nameof(source.LocalNodeId), nameof(target.LocalNodeId));
            CopyProperty(source, target, nameof(source.RemoteNodeId), nameof(target.RemoteNodeId));
            CopyProperty(source, target, nameof(source.Direction), nameof(target.Direction));
            CopyProperty(source, target, nameof(source.Instruction), nameof(target.Instruction));
            CopyProperty(source, target, nameof(source.Status), nameof(target.Status));
            CopyProperty(source, target, nameof(source.Conflict), nameof(target.Conflict));
            CopyProperty(source, target, nameof(source.Inconsistency), nameof(target.Inconsistency));
            CopyProperty(source, target, nameof(source.CancelType), nameof(target.CancelType));
            CopyProperty(source, target, nameof(source.Error), nameof(target.Error));
            CopyProperty(source, target, nameof(source.Size), nameof(target.Size));
            CopyProperty(source, target, nameof(source.Progress), nameof(target.ProgressPercent));
            CopyProperty(source, target, nameof(source.OperationId), nameof(target.OperationId));
        }
    }

    public class NodeInfo
    {
        public NodeId? NodeId { get; set; }
        public string? Name { get; set; }
        public Int64? Size { get; set; }
        public NodeId? ParentNodeId { get; set; }
        public string? Path { get; set; }
        public bool? AccessDenied { get; set; }
    };

    public static partial class ConversionHelper
    {
        public static void CopyToNode(NodeInfo source, Node target)
        {
            CopyProperty(source, target, nameof(source.NodeId), nameof(target.NodeId));
            CopyProperty(source, target, nameof(source.Name), nameof(target.Name));
            CopyProperty(source, target, nameof(source.Size), nameof(target.Size));
            CopyProperty(source, target, nameof(source.ParentNodeId), nameof(target.ParentNodeId));
            CopyProperty(source, target, nameof(source.Path), nameof(target.Path));
            CopyProperty(source, target, nameof(source.AccessDenied), nameof(target.AccessDenied));
        }
    }

    public class NodeConflictInfo
    {
        public string? AuthorName { get; set; }
        public Int64? FileSize { get; set; }
        public DateTime? LastModificationDate { get; set; }
    }

    public class ErrorInfo
    {
        public DbId? DbId { get; set; }
        public DateTime? Time { get; set; }
        public ErrorLevel? Level { get; set; }
        public DbId? SyncDbId { get; set; }
        public ExitCode? ExitCode { get; set; }
        public ExitCause? ExitCause { get; set; }
        public NodeType? NodeType { get; set; }
        public string? Path { get; set; }
        public string? DestinationPath { get; set; }
        public NodeId? LocalNodeId { get; set; }
        public NodeId? RemoteNodeId { get; set; }
        public ConflictType? ConflictType { get; set; }
        public InconsistencyType? InconsistencyType { get; set; }
        public CancelType? CancelType { get; set; }
        public bool? AutoResolved { get; set; }
    }

    public static partial class ConversionHelper
    {
        public static void CopyToError(ErrorInfo source, Error target)
        {
            CopyProperty(source, target, nameof(source.DbId), nameof(target.DbId));
            CopyProperty(source, target, nameof(source.Time), nameof(target.Timestamp));
            CopyProperty(source, target, nameof(source.Level), nameof(target.ErrorLevel));
            CopyProperty(source, target, nameof(source.ExitCode), nameof(target.ExitCode));
            CopyProperty(source, target, nameof(source.ExitCause), nameof(target.ExitCause));
            CopyProperty(source, target, nameof(source.NodeType), nameof(target.NodeType));
            CopyProperty(source, target, nameof(source.Path), nameof(target.Path));
            CopyProperty(source, target, nameof(source.DestinationPath), nameof(target.DestinationPath));
            CopyProperty(source, target, nameof(source.ConflictType), nameof(target.ConflictType));
            CopyProperty(source, target, nameof(source.InconsistencyType), nameof(target.InconsistencyType));
            CopyProperty(source, target, nameof(source.CancelType), nameof(target.CancelType));
            CopyProperty(source, target, nameof(source.AutoResolved), nameof(target.AutoResolved));
        }
    }

    public class ExclusionTemplateInfo
    {
        public string? Template { get; set; }
        public bool? Default { get; set; }
        public bool? Warning { get; set; }
    }

    public static partial class ConversionHelper
    {
        public static ExclusionTemplate FromExclusionTemplateInfo(ExclusionTemplateInfo source)
        {
            return new ExclusionTemplate(
                template: source.Template ?? string.Empty,
                warning: source.Warning ?? false,
                isDefault: source.Default ?? false
            );
        }

        public static void CopyToExclusionTemplateInfo(ExclusionTemplate source, ExclusionTemplateInfo target)
        {
            CopyProperty(source, target, nameof(source.Template), nameof(target.Template));
            CopyProperty(source, target, nameof(source.Default), nameof(target.Default));
            CopyProperty(source, target, nameof(source.Warning), nameof(target.Warning));
        }
    }

    public class SearchInfo
    {
        public NodeId? Id { get; set; }
        public string? Name { get; set; }
        public NodeType? Type { get; set; }
        public string? Path { get; set; }
        public DateTime? ModifiedTime { get; set; }
        public Int64? Size { get; set; }
        public bool? IsAvailableLocally { get; set; }
    }
}
