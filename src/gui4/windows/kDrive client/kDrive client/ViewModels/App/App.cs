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

using DynamicData;
using DynamicData.Binding;
using Infomaniak.kDrive.Pages.Settings;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Dispatching;
using System;
using System.Collections.ObjectModel;
using System.Linq;
using System.Reactive.Linq;
using System.Threading;
using System.Threading.Tasks;
using Windows.Networking.Connectivity;

namespace Infomaniak.kDrive.ViewModels

{
    public class AppModel : UISafeObservableObject
    {
        /** Indicates if the model has been initialized (i.e. data loaded from the server)
         *  This is only set to true after InitializeAsync() has been called and completed.
         *  It is false by default.
         */
        private bool _isInitialized = false;

        /** The currently selected sync in the UI.
         *  This can be null if no sync is selected.
         */
        private Sync? _selectedSync = null;

        /** The list of users setup in the application.
         *  This is an observable collection, so the UI can bind to it and be notified of changes.
         */
        private ObservableCollection<User> _users = [];

        /** The dispatcher queue for the UI thread.
         *  This is used to marshal calls to the UI thread when updating observable item.
         *  It must be set in the mainWindow constructor.
         */
        public static DispatcherQueue UIThreadDispatcher { get; set; } = Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread();

        // The list of server level error
        public ObservableCollection<Error> AppErrors = [];

        // Helpers - Agregated collections
        /** The list of active syncs across all users.
        *  This is a read-only observable collection, so the UI can bind to it and be notified of changes.
        *  It is automatically updated when a sync's IsActive property changes or when syncs are added/removed from users.
        */
        public ReadOnlyObservableCollection<Sync> AllSyncs { get; set; }

        public ReadOnlyObservableCollection<Drive> AllDrives { get; set; }

        // Application settings
        public Settings Settings { get; } = new Settings();

        // Helpers
        private readonly Task? _networkWatcher;
        private readonly CancellationTokenSource _networkWatcherCancellationSource = new();
        private bool _networkAvailable = true;
        private bool _updateRequired;

        public class SelectedSyncChangedEventArgs : EventArgs
        {
            public Sync? OldValue { get; }
            public Sync? NewValue { get; }

            public SelectedSyncChangedEventArgs(Sync? oldValue, Sync? newValue)
            {
                OldValue = oldValue;
                NewValue = newValue;
            }
        }
        public event EventHandler<SelectedSyncChangedEventArgs>? SelectedSyncChanged;

        public Sync? SelectedSync
        {
            get => _selectedSync;
            set
            {
                var oldValue = _selectedSync;
                if (SetPropertyInUIThread(ref _selectedSync, value))
                {
                    SelectedSyncChanged?.Invoke(this, new SelectedSyncChangedEventArgs(oldValue, _selectedSync));
                }
            }
        }

        public AppModel()
        {
            // Create a read-only observable collection of active drives across all users
            _users.ToObservableChangeSet()
                .AutoRefresh(u => u.Accounts.Count)
                .TransformMany(a => a.Accounts)
                .AutoRefresh(a => a.Drives.Count)
                .TransformMany(a => a.Drives)
                .AutoRefresh(d => d.Syncs.Count)
                .TransformMany(d => d.Syncs)
                .Sort(SortExpressionComparer<Sync>.Ascending(s => s.Drive.DbId))
                .Bind(out var allSyncs)
                .Subscribe();
            AllSyncs = allSyncs;

            // Create a read-only observable collection of all drives across all users
            _users.ToObservableChangeSet()
               .AutoRefresh(u => u.Accounts.Count)
               .TransformMany(a => a.Accounts)
               .AutoRefresh(a => a.Drives.Count)
               .TransformMany(a => a.Drives)
               .Sort(SortExpressionComparer<Drive>.Ascending(d => d.DbId))
               .Bind(out var allDrives)
               .Subscribe();
            AllDrives = allDrives;

            // Observe changes to ActiveDrives list and ensure SelectedSync is valid
            AllSyncs.ToObservableChangeSet()
                                       .Subscribe(_ => UIThreadDispatcher.TryEnqueue(EnsureValidSelectedSync));

            _networkWatcher = Task.Run(() => WatchNetworkAsync(_networkWatcherCancellationSource.Token));
        }

        ~AppModel()
        {
            _networkWatcherCancellationSource.Cancel();
            _networkWatcher?.Wait();
            _networkWatcherCancellationSource.Dispose();
        }
        private async Task WatchNetworkAsync(CancellationToken cancellationToken)
        {
            bool previousStatus = _networkAvailable;

            while (!cancellationToken.IsCancellationRequested)
            {
                bool isAvailable = false;

                try
                {
                    var profile = NetworkInformation.GetInternetConnectionProfile();

                    if (profile != null)
                    {
                        var level = profile.GetNetworkConnectivityLevel();
                        isAvailable = level == NetworkConnectivityLevel.InternetAccess;
                    }
                }
                catch
                {
                    // Any unexpected error -> assume online to avoid blocking the user
                    isAvailable = true;
                }

                if (previousStatus != isAvailable)
                {
                    previousStatus = isAvailable;
                    NetworkAvailable = isAvailable;
                }

                try
                {
                    await Task.Delay(TimeSpan.FromSeconds(5), cancellationToken).ConfigureAwait(false);
                }
                catch (TaskCanceledException)
                {
                    break;
                }
            }
        }

        private void EnsureValidSelectedSync()
        {
            // If AllSync is empty, set SelectedSync to null
            if (AllSyncs.Count == 0)
            {
                Logger.Log(Logger.Level.Debug, "There are no syncs available, setting SelectedSync to null.");
                SelectedSync = null;
                ((App.Current as App)?.CurrentWindow as MainWindow)?.AppNavView?.Frame?.Navigate(typeof(SettingsPage));
            }
            else if (_selectedSync == null || (_selectedSync != null && !AllSyncs.Contains(_selectedSync))) // If SelectedSync is null or not in AllSyncs, pick the first one
            {
                Logger.Log(Logger.Level.Debug, "SelectedSync is null or not in AllSyncs, selecting the first available sync.");
                SelectedSync = AllSyncs[0];
            }
        }

        public ObservableCollection<User> Users
        {
            get => _users;
            set => SetPropertyInUIThread(ref _users, value);
        }

        public bool IsInitialized
        {
            get => _isInitialized;
            set => SetPropertyInUIThread(ref _isInitialized, value);
        }

        public bool NetworkAvailable
        {
            get => _networkAvailable;
            set => SetPropertyInUIThread(ref _networkAvailable, value);
        }

        public bool UpdateRequired
        {
            get => _updateRequired;
            set => SetPropertyInUIThread(ref _updateRequired, value);
        }

        /** Initialize the model by loading data from the server.
         *  This method is asynchronous and should be awaited.
         *  It loads the list of users and their properties/drives from the server.
         */
        public async Task<bool> InitializeAsync()
        {

            Logger.Log(Logger.Level.Info, "Initializing AppModel...");
            Users.Clear();
            SelectedSync = null;

            IServerCommService serverCommService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            try
            {
                // Allow up to 5 minutes for the initial load as it happen at computer startup which can be slow
                using (var cts = new CancellationTokenSource(TimeSpan.FromMinutes(5)))
                {
                    if (!await serverCommService.RefreshUsers(cts.Token))
                    {
                        Logger.Log(Logger.Level.Error, "Failed to refresh users during AppModel initialization.");
                        return false;
                    }

                    if (!await serverCommService.RefreshAccounts(cts.Token))
                    {
                        Logger.Log(Logger.Level.Error, "Failed to refresh accounts during AppModel initialization.");
                        return false;
                    }

                    if (!await serverCommService.RefreshDrives(cts.Token))
                    {
                        Logger.Log(Logger.Level.Error, "Failed to refresh drives during AppModel initialization.");
                        return false;
                    }

                    if (!await serverCommService.RefreshSyncs(cts.Token))
                    {
                        Logger.Log(Logger.Level.Error, "Failed to refresh syncs during AppModel initialization.");
                        return false;
                    }

                    if (!await serverCommService.RefreshSettings(cts.Token))
                    {
                        Logger.Log(Logger.Level.Error, "Failed to refresh settings during AppModel initialization.");
                        return false;
                    }

                    if (!await serverCommService.RefreshErrors(cts.Token))
                    {
                        Logger.Log(Logger.Level.Error, "Failed to refresh errors during AppModel initialization.");
                        return false;
                    }

                    Logger.Log(Logger.Level.Info, "All server data loaded successfully.");
                    IsInitialized = true;


                    // Refresh updater version info and load info in parallel, they are not critical for the app to function and can be slow to load
                    _ = Task.Run(async () =>
                    {
                        if (!await serverCommService.RefreshUpdaterVersionInfo(null, CancellationToken.None))
                            Logger.Log(Logger.Level.Warning, "RefreshUpdaterVersionInfo returned false during AppModel initialization.");

                    });

                    _ = Task.Run(async () =>
                    {
                        if (!await serverCommService.ActivateLoadInfo(CancellationToken.None))
                            Logger.Log(Logger.Level.Warning, "Failed to ActivateLoadInfo during AppModel initialization.");
                    });
                    return true;
                }
            }
            catch (OperationCanceledException)
            {
                Logger.Log(Logger.Level.Error, "Operation canceled during AppModel initialization.");
                return false;
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Error, $"Exception during AppModel initialization: {ex}");
                return false;
            }
        }

        public async Task<bool> DisconnectUserAsync(DbId userDbId)
        {
            IServerCommService serverCommService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            if (!await serverCommService.RemoveUser(userDbId, CancellationToken.None))
            {
                Logger.Log(Logger.Level.Error, $"Failed to disconnect user {userDbId}");
                return false;
            }
            return true;
        }

        public async Task AddErrorAsync(Error error)
        {
            Logger.Log(Logger.Level.Info, $"AppModel: Adding error - {error}");
            if (error.ErrorLevel == Types.ErrorLevel.Server || error.ExitCode == ExitCode.UpdateRequired) // Treat any UpdateRequired error as a Server level error
            {
                error.ErrorLevel = ErrorLevel.Server;
                await Utility.RunOnUIThread(() => AppErrors.Add(error));
                await RefreshErrorState();
                return;
            }

            if (error.Sync == null)
            {
                Logger.Log(Logger.Level.Error, $"AppModel: Cannot add sync error without associated sync - {error}");
                return;
            }
            await error.Sync.AddErrorAsync(error);
        }

        public async Task RemoveErrorByDbIdAsync(DbId errorDbId)
        {
            Logger.Log(Logger.Level.Info, $"AppModel: Removing error - {errorDbId}");
            var appError = AppErrors.FirstOrDefault(e => e.DbId == errorDbId);
            if (appError is not null)
            {
                await Utility.RunOnUIThread(() => AppErrors.Remove(appError));
                await RefreshErrorState();
                return;
            }

            foreach (var sync in AllSyncs)
            {
                var syncError = sync.SyncErrors.FirstOrDefault(e => e.DbId == errorDbId);
                if (syncError != null)
                {
                    await sync.RemoveErrorAsync(syncError);
                    return;
                }
            }

            Logger.Log(Logger.Level.Warning, $"AppModel: Could not find error with DbId {errorDbId} to remove.");
        }

        public async Task ClearAllErrorsAsync()
        {
            Logger.Log(Logger.Level.Info, "AppModel: Clearing all errors.");
            foreach (var appError in AppErrors)
            {
                await RemoveErrorByDbIdAsync(appError.DbId);
            }

            foreach (var sync in AllSyncs)
            {
                await sync.ClearAllErrorsAsync();
            }
        }

        public async Task RefreshErrorState()
        {
            await Utility.RunOnUIThread(() => UpdateRequired = AppErrors.Any(e => e.ExitCode == Types.ExitCode.UpdateRequired));
        }
    }
}
