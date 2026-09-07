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
global using AccountId = System.Int64;
global using DbId = System.Int64;
global using DriveId = System.Int64;
global using NodeId = System.String;
global using SyncPath = System.String;
global using UserId = System.Int64;
using System.Collections.Generic;
using System.ComponentModel;
using System.Threading.Tasks;


namespace Infomaniak.kDrive.Types
{

    public enum NodeType
    {
        Unknown,
        File, // File or symlink
        Directory
    }

    public enum SyncStatus
    {
        Undefined,
        Starting,
        Running,
        Idle,
        PauseAsked,
        Paused,
        StopAsked,
        Stopped,
        Error,
        Offline
    };

    public enum SyncErrorStates
    {
        Undefined,
        Asleep,
        WakingUp,
        NotRenew,
        Maintenance,
        AccessDenied,
        LoggingError
    };

    public enum SyncFileInstruction
    {
        None,
        Update,
        UpdateMetadata,
        Remove,
        Move,
        Get,
        Put,
        Ignore
    };
    public enum SyncFileStatus
    {
        Unknown,
        Error,
        Success,
        Conflict,
        Inconsistency,
        Ignored,
        Syncing
    };

    public enum SyncDirection
    {
        Unknown,
        Up,
        Down
    };

    public enum SyncType
    {
        Unknown,
        Offline,
        Online // (Ex liteSync)
    };

    public enum DistributionChannel
    {
        Prod,
        Next,
        Beta,
        Internal,
        Legacy,
        Test,
        Unknown
    };

    public enum OAuth2State
    {
        None,
        WaitingForUserAction,
        ProcessingResponse,
        Success,
        Error
    }
    public interface IDrive : INotifyPropertyChanged
    {
        public string Name { get; }
        public System.Drawing.Color Color { get; }
        public DriveId DriveId { get; set; }
        public DbId UserDbId { get; }
        public AccountId AccountId { get; }
        public string AccountName { get; }
        public bool IsConfigured { get; } // Indicates if at least one sync (which is not an advanced sync) is set up for this drive
    }

    public partial class DriveAvailable : UISafeObservableObject, IDrive
    {
        public DriveId DriveId { get; set; } = 0;
        public UserId UserId { get; set; } = 0;
        public DbId UserDbId { get; set; } = 0;
        public AccountId AccountId { get; set; } = 0;
        public string AccountName { get; set; } = "";
        public string Name { get; set; } = "";
        public System.Drawing.Color Color { get; set; } = System.Drawing.Color.White;
        public bool IsConfigured { get; } = false;
    }

    public interface ISync
    {
        public IDrive Drive { get; }
        public SyncPath LocalPath { get; }
        public SyncPath RemotePath { get; }
        public NodeId RemoteNodeId { get; }
        public SyncType SyncType { get; }

        public Task<List<NodeId>?> GetExcludedNodeIds();
    }
}

