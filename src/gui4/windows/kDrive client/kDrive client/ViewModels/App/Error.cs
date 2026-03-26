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

using Infomaniak.kDrive.ServerCommunication.CommStruct;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Microsoft.Extensions.DependencyInjection;
using System;
using System.Threading;
using System.Threading.Tasks;
using Windows.System;

namespace Infomaniak.kDrive.ViewModels
{
    public class Error : UISafeObservableObject
    {
        private DbId _DbId = -1;
        private DateTime _timestamp = DateTime.MinValue;
        private ErrorLevel _errorLevel = ErrorLevel.Unknown;
        private Sync? _sync;
        private ExitCode _exitCode = ExitCode.Unknown;
        private ExitCause _exitCause = ExitCause.Unknown;
        NodeType _nodeType = NodeType.Unknown;
        private string _path = "";
        private string _destinationPath = "";
        private NodeId _localNodeId = "";
        private NodeId _remoteNodeId = "";
        private ConflictType _conflictType = ConflictType.None;
        private InconsistencyType _inconsistencyType = InconsistencyType.None;
        private CancelType _cancelType = CancelType.None;
        bool _autoResolved = false;

        // UI properties
        public string NodeTypeTranslationKey => NodeType switch
        {
            NodeType.File => "labelFileLowerCase",
            NodeType.Directory => "labelFolderLowerCase",
            _ => "labelFileLowerCase"
        };

        public Error(Sync sync)
        {
            _sync = sync;
        }

        public Error(Sync sync, ErrorInfo errorInfo)
        {
            _DbId = errorInfo.DbId ?? _DbId;
            _timestamp = errorInfo.Time ?? _timestamp;
            _errorLevel = errorInfo.Level ?? _errorLevel;
            _exitCode = errorInfo.ExitCode ?? _exitCode;
            _exitCause = errorInfo.ExitCause ?? _exitCause;
            _nodeType = errorInfo.NodeType ?? _nodeType;
            _path = errorInfo.Path ?? _path;
            _destinationPath = errorInfo.DestinationPath ?? _destinationPath;
            _localNodeId = errorInfo.LocalNodeId ?? _localNodeId;
            _remoteNodeId = errorInfo.RemoteNodeId ?? _remoteNodeId;
            _conflictType = errorInfo.ConflictType ?? _conflictType;
            _inconsistencyType = errorInfo.InconsistencyType ?? _inconsistencyType;
            _cancelType = errorInfo.CancelType ?? _cancelType;
            _autoResolved = errorInfo.AutoResolved ?? _autoResolved;
            _sync = sync;
        }
        public Error(ErrorInfo errorInfo)
        {
            _DbId = errorInfo.DbId ?? _DbId;
            _timestamp = errorInfo.Time ?? _timestamp;
            _errorLevel = errorInfo.Level ?? _errorLevel;
            _exitCode = errorInfo.ExitCode ?? _exitCode;
            _exitCause = errorInfo.ExitCause ?? _exitCause;
            _nodeType = errorInfo.NodeType ?? _nodeType;
            _path = errorInfo.Path ?? _path;
            _destinationPath = errorInfo.DestinationPath ?? _destinationPath;
            _localNodeId = errorInfo.LocalNodeId ?? _localNodeId;
            _remoteNodeId = errorInfo.RemoteNodeId ?? _remoteNodeId;
            _conflictType = errorInfo.ConflictType ?? _conflictType;
            _inconsistencyType = errorInfo.InconsistencyType ?? _inconsistencyType;
            _cancelType = errorInfo.CancelType ?? _cancelType;
            _autoResolved = errorInfo.AutoResolved ?? _autoResolved;
        }

        public DbId DbId
        {
            get => _DbId;
            set => SetPropertyInUIThread(ref _DbId, value);
        }

        public DateTime Timestamp
        {
            get => _timestamp;
            set => SetPropertyInUIThread(ref _timestamp, value);
        }

        public ErrorLevel ErrorLevel
        {
            get => _errorLevel;
            set => SetPropertyInUIThread(ref _errorLevel, value);
        }

        public Sync? Sync
        {
            get => _sync;
            private set => SetPropertyInUIThread(ref _sync, value);
        }

        public ExitCode ExitCode
        {
            get => _exitCode;
            set => SetPropertyInUIThread(ref _exitCode, value);
        }

        public ExitCause ExitCause
        {
            get => _exitCause;
            set => SetPropertyInUIThread(ref _exitCause, value);
        }

        public NodeType NodeType
        {
            get => _nodeType;
            set => SetPropertyInUIThread(ref _nodeType, value);
        }

        public string Path
        {
            get => _path;
            set => SetPropertyInUIThread(ref _path, value.Replace('\\', '/'));
        }

        public string DestinationPath
        {
            get => _destinationPath;
            set => SetPropertyInUIThread(ref _destinationPath, value.Replace('\\', '/'));
        }

        public NodeId LocalNodeId
        {
            get => _localNodeId;
            set => SetPropertyInUIThread(ref _localNodeId, value);
        }

        public NodeId RemoteNodeId
        {
            get => _remoteNodeId;
            set => SetPropertyInUIThread(ref _remoteNodeId, value);
        }

        public ConflictType ConflictType
        {
            get => _conflictType;
            set => SetPropertyInUIThread(ref _conflictType, value);
        }

        public InconsistencyType InconsistencyType
        {
            get => _inconsistencyType;
            set => SetPropertyInUIThread(ref _inconsistencyType, value);
        }

        public CancelType CancelType
        {
            get => _cancelType;
            set => SetPropertyInUIThread(ref _cancelType, value);
        }

        public bool AutoResolved
        {
            get => _autoResolved;
            set => SetPropertyInUIThread(ref _autoResolved, value);
        }

        public async Task<NodeConflictInfo?> GetConflictNodeVersionInfo(ReplicaSide side, CancellationToken cancellationToken)
        {
            if (Sync is null) return null;
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            var path = side == ReplicaSide.Local ? DestinationPath : Path;
            return await commService.GetNodeConflictInfo(Sync.DbId, path, side, cancellationToken);
        }

        public bool IsConflictUserResolvable()
        {
            return ErrorLevel == ErrorLevel.Node && IsConflictUserResolvable(ConflictType);
        }

        public static bool IsConflictUserResolvable(ConflictType conflictType)
        {
            return conflictType == ConflictType.CreateCreate || conflictType == ConflictType.EditEdit;
        }

        public async Task<bool> OpenItemInWebViewAsync()
        {
            if (string.IsNullOrEmpty(RemoteNodeId))
            {
                Logger.Log(Logger.Level.Error, "Error RemoteNodeId is null or empty. Cannot navigate to node.");
                return false;
            }

            try
            {
                if (Sync is null)
                {
                    Logger.Log(Logger.Level.Error, "Sync is null");
                    return false;
                }

                if (!await Launcher.LaunchUriAsync(App.Constants.Drive.itemUri(Sync.Drive.DriveId, RemoteNodeId)))
                {
                    Logger.Log(Logger.Level.Error, $"Failed to launch URI for node with RemoteNodeId: {RemoteNodeId}");
                    return false;
                }
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Error, $"Failed to launch URI for node with RemoteNodeId: {RemoteNodeId}. Exception: {ex.Message}");
                return false;
            }
            return true;
        }
    }
}