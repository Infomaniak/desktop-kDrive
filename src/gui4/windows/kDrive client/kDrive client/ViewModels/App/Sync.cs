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

using DynamicData.Binding;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Microsoft.Extensions.DependencyInjection;
using System;
using System.Collections.ObjectModel;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public class Sync : UISafeObservableObject
    {
        // Sync properties
        private DbId _dbId;
        private readonly Drive _drive;
        private SyncId _id = -1;
        private SyncPath _localPath = "";
        private SyncPath _remotePath = "";
        private bool _supportOnlineMode = true;
        private readonly ObservableCollection<SyncFileItem> _syncActivities = new();
        private SyncStatus _syncStatus = SyncStatus.Paused;
        private SyncType _syncType = SyncType.Unknown;
        private bool _isTypeOnline = false;
        private bool _syncTypeMigrationInProgress = false;
        private SyncFileItem? _lastActivity;

        // Sync UI properties
        private bool _showIncomingActivity = true;

        public SyncStatus SyncStatus
        {
            get
            {
                if (_syncStatus == SyncStatus.Paused && !App.ServiceProvider.GetRequiredService<AppModel>().NetworkAvailable)
                {
                    return SyncStatus.Offline;
                }
                return _syncStatus;
            }
            set => SetPropertyInUIThread(ref _syncStatus, value);
        }

        public Sync(DbId dbId, Drive drive)
        {
            _dbId = dbId;
            _drive = drive;

            App.ServiceProvider.GetRequiredService<AppModel>().WhenAnyPropertyChanged("NetworkAvailable").Subscribe(appModel =>
            {
                if (SyncStatus == SyncStatus.Paused || SyncStatus == SyncStatus.Offline)
                {
                    bool networkAvailable = appModel?.NetworkAvailable ?? true;
                    SyncStatus = networkAvailable ? SyncStatus.Paused : SyncStatus.Offline;
                }
            });

            SyncActivities.CollectionChanged += (s, args) =>
            {
                try
                {
                    LastActivity = SyncActivities[0];
                }
                catch (ArgumentOutOfRangeException)
                {
                    LastActivity = null;
                }

            };

            // Fake sync level error for testing
            Error testSyncError = new();
            testSyncError.DbId = 1;
            testSyncError.SyncDbId = DbId;
            testSyncError.Timestamp = DateTime.Now;
            testSyncError.ErrorLevel = ErrorLevel.SyncPal;
            testSyncError.ExitCode = ExitCode.SystemError;
            testSyncError.ExitCause = ExitCause.SyncDirAccessError;
            AddError(testSyncError);

            // Fake node level error for testing
            Error testNodeError = new();
            testNodeError.DbId = 2;
            testNodeError.SyncDbId = DbId;
            testNodeError.Timestamp = DateTime.Now;
            testNodeError.ErrorLevel = ErrorLevel.Node;
            testNodeError.Path = "/path/to/error/file.txt";
            testNodeError.ExitCode = ExitCode.SystemError;
            testNodeError.ExitCause = ExitCause.FileAccessError;
            AddError(testNodeError);
        }

        public DbId DbId
        {
            get => _dbId;
            set => SetPropertyInUIThread(ref _dbId, value);
        }

        public SyncId Id
        {
            get => _id;
            set => SetPropertyInUIThread(ref _id, value);
        }

        public SyncPath LocalPath
        {
            get => _localPath;
            set => SetPropertyInUIThread(ref _localPath, value);

        }

        public SyncPath RemotePath
        {
            get => _remotePath;
            set
            {
                // Ensure the path ends with a directory separator
                if (!string.IsNullOrEmpty(_remotePath) && !_remotePath.EndsWith(Path.DirectorySeparatorChar.ToString()))
                {
                    _remotePath += Path.DirectorySeparatorChar;
                }
                SetPropertyInUIThread(ref _remotePath, value);
            }
        }

        public bool SupportOnlineMode
        {
            get => _supportOnlineMode;
            set => SetPropertyInUIThread(ref _supportOnlineMode, value);
        }

        public SyncType SyncType
        {
            get => _syncType;
            set
            {
                SetPropertyInUIThread(ref _syncType, value);
                SetPropertyInUIThread(ref _isTypeOnline, value == SyncType.Online, nameof(IsTypeOnline));
            }
        }

        public bool SyncTypeMigrationInProgress
        {
            get => _syncTypeMigrationInProgress;
        }

        public bool IsTypeOnline
        {
            get => _isTypeOnline;
        }

        public ObservableCollection<SyncFileItem> SyncActivities
        {
            get => _syncActivities;
        }

        // The list of sync and node errors
        public ObservableCollection<Error> SyncErrors = new();

        public Drive Drive
        {
            get => _drive;
        }

        public SyncFileItem? LastActivity
        {
            get => _lastActivity;
            set => SetPropertyInUIThread(ref _lastActivity, value);
        }

        public bool ShowIncomingActivity
        {
            get => _showIncomingActivity;
            set => SetPropertyInUIThread(ref _showIncomingActivity, value);
        }

        public async Task Start()
        {
            var serverComm = App.ServiceProvider.GetRequiredService<IServerCommService>();
            await serverComm.StartSync(DbId, CancellationToken.None);
        }

        public async Task Pause()
        {
            var serverComm = App.ServiceProvider.GetRequiredService<IServerCommService>();
            await serverComm.PauseSync(DbId, CancellationToken.None);
        }

        public async Task ChangeSyncType(SyncType newType)
        {
            SetPropertyInUIThread(ref _syncTypeMigrationInProgress, true, nameof(SyncTypeMigrationInProgress));
            Logger.Log(Logger.Level.Error, "Changing sync type is not yet implemented.");
            await Task.Delay(5000); // TODO: Replace with actual implementation
            SetPropertyInUIThread(ref _syncTypeMigrationInProgress, false, nameof(SyncTypeMigrationInProgress));

        }

        public void AddError(Error error)
        {
            if (error.ErrorLevel != Types.ErrorLevel.SyncPal && error.ErrorLevel != Types.ErrorLevel.Node)
            {
                Logger.Log(Logger.Level.Error, $"Sync {DbId}: Ignoring error {error.ExitCode} - {error.Path} with level {error.ErrorLevel}");
                return;
            }

            Logger.Log(Logger.Level.Info, $"Sync {DbId}: Adding error {error.ExitCode} - {error.Path}");
            SyncErrors.Add(error);
        }

        public void RemoveError(Error error)
        {
            Logger.Log(Logger.Level.Info, $"Sync {DbId}: Removing error {error.ExitCode} - {error.Path}");
            if (!SyncErrors.Remove(error)){
                Logger.Log(Logger.Level.Warning, $"Sync {DbId}: Tried to remove non-existing error {error.ExitCode} - {error.Path}");
            }
        }
    }
}
