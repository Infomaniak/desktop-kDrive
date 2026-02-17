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
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public class Sync : UISafeObservableObject, ISync
    {
        // Sync properties
        private DbId _dbId;
        private readonly Drive _drive;
        private SyncPath _localPath = "";
        private SyncPath _remotePath = "";
        private NodeId _remoteNodeId = "";
        private bool _supportOnlineMode = true;
        private SyncStatus _syncStatus = SyncStatus.Paused;
        private SyncType _syncType = SyncType.Unknown;
        private bool _isTypeOnline = false;

        // Sync UI properties
        private bool _showIncomingActivity = true;
        private SyncErrorStates _syncErrorState = SyncErrorStates.Undefined;
        private readonly ObservableCollection<SyncFileItem> _syncActivities = new();
        private bool _syncTypeMigrationInProgress = false;
        private SyncFileItem? _lastActivity;


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
                if (!SyncActivities.Any())
                {
                    LastActivity = null;
                    return;
                }
                try
                {
                    LastActivity = SyncActivities[0];
                }
                catch (ArgumentOutOfRangeException)
                {
                    LastActivity = null;
                }
            };
        }

        public DbId DbId
        {
            get => _dbId;
            set => SetPropertyInUIThread(ref _dbId, value);
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

        public NodeId RemoteNodeId
        {
            get => _remoteNodeId;
            set => SetPropertyInUIThread(ref _remoteNodeId, value);
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
            set
            {
                SetPropertyInUIThread(ref _syncTypeMigrationInProgress, value);
            }
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

        public SyncErrorStates SyncErrorState
        {
            get => _syncErrorState;
            private set => SetPropertyInUIThread(ref _syncErrorState, value);
        }

        public Drive Drive
        {
            get => _drive;
        }
        IDrive ISync.Drive => _drive;

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

        public async Task<bool> Start()
        {
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            return await commService.StartSync(DbId, CancellationToken.None);
        }

        public async Task<bool> Pause()
        {
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            return await commService.PauseSync(DbId, CancellationToken.None);
        }

        public async Task<bool> ChangeSyncType(SyncType newType)
        {
            SyncTypeMigrationInProgress = true;
            var previousType = SyncType;
            SyncType = newType;
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            bool result = await commService.SetSyncType(DbId, newType, CancellationToken.None);
            if (!result)
                SyncType = previousType;
            SyncTypeMigrationInProgress = false;
            return result;
        }

        public async Task AddErrorAsync(Error error)
        {
            if (error.ErrorLevel != Types.ErrorLevel.SyncPal && error.ErrorLevel != Types.ErrorLevel.Node)
            {
                Logger.Log(Logger.Level.Error, $"Sync {DbId}: Ignoring error {error.ExitCode} - {error.Path} with level {error.ErrorLevel}");
                return;
            }

            Logger.Log(Logger.Level.Info, $"Sync {DbId}: Adding error {error.ExitCode} - {error.Path}");
            await Utility.RunOnUIThread(() => SyncErrors.Add(error));

            if (error.ExitCause == ExitCause.QuotaExceeded)
                Drive.DisplayRemoteSpaceWarning = true;

            await RefreshErrorState();
        }

        public async Task RemoveErrorAsync(Error error, bool refreshErrorState = true)
        {
            Logger.Log(Logger.Level.Info, $"Sync {DbId}: Removing error {error.ExitCode} - {error.Path}");
            await Utility.RunOnUIThread(() =>
            {
                if (!SyncErrors.Remove(error))
                    Logger.Log(Logger.Level.Warning, $"Sync {DbId}: Tried to remove non-existing error {error.ExitCode} - {error.Path}");
            });

            if (refreshErrorState)
                await RefreshErrorState();
        }

        public async Task ClearAllErrorsAsync()
        {
            await Utility.RunOnUIThread(async () =>
            {
                // Call RemoveError for each error to ensure proper handling (RemoveError is responsible for some viewmodel updates)
                while (SyncErrors.Any())
                {
                    var error = SyncErrors[0];
                    Logger.Log(Logger.Level.Info, $"Sync {DbId}: Clearing error {error.ExitCode} - {error.Path}");
                    await RemoveErrorAsync(error, false);
                }
            });
            await RefreshErrorState();
        }

        public async Task RefreshErrorState()
        {
            await Utility.RunOnUIThread(async () =>
            {

                SyncErrorState = SyncErrorStates.Undefined;

                foreach (var error in SyncErrors)
                {
                    SyncErrorState = error.ExitCause switch
                    {
                        Types.ExitCause.DriveAccessError => SyncErrorStates.AccessDenied,
                        Types.ExitCause.DriveAsleep => SyncErrorStates.Asleep,
                        Types.ExitCause.DriveWakingUp => SyncErrorStates.WakingUp,
                        Types.ExitCause.DriveMaintenance => SyncErrorStates.Maintenance,
                        Types.ExitCause.DriveNotRenew => SyncErrorStates.NotRenew,
                        Types.ExitCause.LoginError => SyncErrorStates.LoggingError,
                        _ => SyncErrorStates.Undefined
                    };
                    if (error.ExitCode == ExitCode.InvalidToken)
                    {
                        SyncErrorState = SyncErrorStates.LoggingError;
                    }

                    if (SyncErrorState != SyncErrorStates.Undefined)
                    {
                        Logger.Log(Logger.Level.Info, $"Sync {DbId}: Setting SyncErrorState to {SyncErrorState} based on error {error.ExitCode} - {error.Path}");
                        return;
                    }
                }

                if (SyncErrorState == SyncErrorStates.Undefined && !Drive.Account.User.IsConnected)
                {
                    //SyncErrorState = SyncErrorStates.LoggedOut;
                }
            });
        }
    }
}