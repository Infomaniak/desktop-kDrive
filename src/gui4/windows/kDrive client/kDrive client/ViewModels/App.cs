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

using CommunityToolkit.Mvvm.ComponentModel;
using DynamicData;
using DynamicData.Binding;
using KDrive.ServerCommunication;
using Microsoft.UI.Dispatching;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Collections.Specialized;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace KDrive.ViewModels

{
    internal class AppModel : ObservableObject
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
        private ObservableCollection<User> _users = new();

        /** The dispatcher queue for the UI thread.
         *  This is used to marshal calls to the UI thread when updating observable item.
         *  It must be set in the mainWindow constructor.
         */
        public static DispatcherQueue UIThreadDispatcher { get; set; } = Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread();

        /** The list of application errors.
         *  This is an observable collection, so the UI can bind to it and be notified of changes.
         */
        private ObservableCollection<Errors.AppError> _appErrors = new();

        /** Indicates if there is error in application or in the selected sync.
         *  This is true if there is at least one app errors or one selected sync error.
         *  This is a read-only property that is automatically updated when AppErrors or SelectedSync.SyncErrors change.
         */
        public bool HasErrors => AppErrors.Count > 0 ||
                           (SelectedSync?.SyncErrors?.Count ?? 0) > 0;

        // Helpers - Agregated collections
        /** The list of active syncs across all users.
        *  This is a read-only observable collection, so the UI can bind to it and be notified of changes.
        *  It is automatically updated when a sync's IsActive property changes or when syncs are added/removed from users.
        */
        public ReadOnlyObservableCollection<Sync> AllSyncs { get; set; }


        public Sync? SelectedSync
        {
            get => _selectedSync;
            set
            {
                SetProperty(ref _selectedSync, value);
                if (SelectedSync != null)
                {
                    SelectedSync.SyncErrors.CollectionChanged -= SyncErrors_CollectionChanged;
                    SelectedSync.SyncErrors.CollectionChanged += SyncErrors_CollectionChanged;
                }
                OnPropertyChanged(nameof(HasErrors));
            }
        }

        public AppModel()
        {
            // Create a read-only observable collection of active drives across all users
            _users.ToObservableChangeSet()
                .AutoRefresh(u => u.Drives.Count)
                .TransformMany(u => u.Drives)
                .AutoRefresh(d => d.Syncs.Count)
                .Filter(d => (d.Syncs.Any()))
                .TransformMany(d => d.Syncs)
                .Bind(out var allSyncs)
                .Subscribe();
            AllSyncs = allSyncs;

            // Observe changes to ActiveDrives list and ensure SelectedSync is valid
            AllSyncs.ToObservableChangeSet()
                       .Subscribe(_ => EnsureValidSelectedSync());

            // Observe changes to AppErrors and SelectedSync.SyncErrors to update HasNoErrors property
            AppErrors.CollectionChanged += (_, __) => OnPropertyChanged(nameof(HasErrors));
        }

        private void EnsureValidSelectedSync()
        {
            // If AllSync is empty, set SelectedSync to null
            if (AllSyncs.Count == 0)
            {
                SelectedSync = null;
            }
            else if (_selectedSync == null || (_selectedSync != null && !AllSyncs.Contains(_selectedSync))) // If SelectedSync is null or not in AllSyncs, pick the first one
            {
                UIThreadDispatcher.TryEnqueue(() => SelectedSync = AllSyncs[0]);
            }
        }

        public ObservableCollection<User> Users
        {
            get => _users;
            set => SetProperty(ref _users, value);
        }

        public bool IsInitialized
        {
            get => _isInitialized;
            set => SetProperty(ref _isInitialized, value);
        }

        public ObservableCollection<Errors.AppError> AppErrors
        {
            get => _appErrors;
            set => SetProperty(ref _appErrors, value);
        }

        /** Initialize the model by loading data from the server.
         *  This method is asynchronous and should be awaited.
         *  It loads the list of users and their properties/drives from the server.
         */
        public async Task InitializeAsync()
        {
            var userDbIds = await ServerCommunication.CommRequests.GetUserDbIds().ConfigureAwait(false);
            if (userDbIds != null)
            {
                List<Task> reloadTasks = new List<Task>();
                List<User> users = new List<User>();
                for (int i = 0; i < userDbIds.Count; i++)
                {
                    User user = new User(userDbIds[i]);
                    reloadTasks.Add(user.Reload());
                    users.Add(user);
                }
                await Task.WhenAll(reloadTasks).ConfigureAwait(false);
                UIThreadDispatcher.TryEnqueue(() =>
                {
                    Users.AddRange(users);
                });
            }
            UIThreadDispatcher.TryEnqueue(() =>
            {
                IsInitialized = true;
            });
        }

        private void SyncErrors_CollectionChanged(object? sender, NotifyCollectionChangedEventArgs e)
        {
            OnPropertyChanged(nameof(HasErrors));
        }
    }
}
