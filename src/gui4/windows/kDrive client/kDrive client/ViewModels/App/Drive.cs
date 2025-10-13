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

using CommunityToolkit.Mvvm.Collections;
using CommunityToolkit.Mvvm.ComponentModel;
using Infomaniak.kDrive.ServerCommunication;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Drawing;
using System.Linq;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public class Drive : ObservableObject
    {
        private DbId _dbId = -1;
        private DriveId _id = -1;
        private string _name = "";
        private Color _color = Color.Blue;
        private long _size = 0;
        private long _usedSize = 0;
        private bool _isActive = false; // Indicates if the user configured this drive on the current device.
        private bool _isPaidOffer = false; // Indicates if the drive is a paid offer (i.e. myKsuite+/pro +, ...)
        private ObservableCollection<Sync> _syncs = new ObservableCollection<Sync>();
        private bool _hasError = false; // Indicate if any of the syncs has an error
        private User _user;
        public Drive(DbId dbId, User user)
        {
            DbId = dbId;
            _user = user;
        }

        public async Task Reload()
        {
            Logger.Log(Logger.Level.Info, $"Reloading Drive properties for DbId {DbId}...");
            Task[] tasks = new Task[]
            {
               CommRequests.GetDriveId(DbId).ContinueWith(t => { if (t.Result != null) Id = t.Result.Value; }, TaskScheduler.FromCurrentSynchronizationContext()),
               CommRequests.GetDriveName(DbId).ContinueWith(t => { if (t.Result != null) Name = t.Result; }, TaskScheduler.FromCurrentSynchronizationContext()),
               CommRequests.GetDriveColor(DbId).ContinueWith(t => { if (t.Result != null) Color = t.Result.Value; }, TaskScheduler.FromCurrentSynchronizationContext()),
               CommRequests.GetDriveSize(DbId).ContinueWith(t => { if (t.Result != null) Size = t.Result.Value; }, TaskScheduler.FromCurrentSynchronizationContext()),
               CommRequests.GetDriveUsedSize(DbId).ContinueWith(t => { if (t.Result != null) UsedSize = t.Result.Value; }, TaskScheduler.FromCurrentSynchronizationContext()),
               CommRequests.GetDriveIsActive(DbId).ContinueWith(t => { if (t.Result != null) IsActive = t.Result.Value; }, TaskScheduler.FromCurrentSynchronizationContext()),
               CommRequests.GetDriveIsPaidOffer(DbId).ContinueWith(t => { if (t.Result != null) IsPaidOffer = t.Result.Value; }, TaskScheduler.FromCurrentSynchronizationContext()),
               CommRequests.GetDriveSyncsDbIds(DbId).ContinueWith(async t =>
                {
                     if (t.Result != null)
                     {
                          ObservableCollection<Sync> syncs = new ObservableCollection<Sync>();
                          Logger.Log(Logger.Level.Debug, $"Drive (DbId: {DbId}) has {t.Result.Count} sync(s). Reloading sync data...");
                          List<Task> syncTasks = new List<Task>();
                          foreach (var syncDbId in t.Result)
                          {
                            Sync? sync = new Sync(syncDbId, this);
                            Syncs.Add(sync);
                            syncTasks.Add(sync.Reload());
                          }
                          await Task.WhenAll(syncTasks);
                        InitWatchers();
                     }
                }, TaskScheduler.FromCurrentSynchronizationContext()).Unwrap()
            };
            await Task.WhenAll(tasks).ConfigureAwait(false);
            Logger.Log(Logger.Level.Info, $"Drive properties reloaded for DbId {DbId}.");
        }
        public DbId DbId
        {
            get => _dbId;
            set => SetProperty(ref _dbId, value);
        }

        public DriveId Id
        {
            get => _id;
            set => SetProperty(ref _id, value);
        }

        public string Name
        {
            get => _name;
            set => SetProperty(ref _name, value);
        }

        public Color Color
        {
            get => _color;
            set => SetProperty(ref _color, value);
        }
        public long Size
        {
            get => _size;
            set => SetProperty(ref _size, value);
        }

        public long UsedSize
        {
            get => _usedSize;
            set => SetProperty(ref _usedSize, value);
        }

        public bool IsActive
        {
            get => _isActive;
            set
            {
                SetProperty(ref _isActive, value);
            }
        }

        public bool IsPaidOffer
        {
            get => _isPaidOffer;
            set => SetProperty(ref _isPaidOffer, value);
        }

        public ObservableCollection<Sync> Syncs
        {
            get { return _syncs; }
            set => SetProperty(ref _syncs, value);
        }

        public Uri GetWebTrashUri()
        {
            return new Uri($"https://ksuite.infomaniak.com/kdrive/app/drive/{Id}/trash");
        }

        public Uri GetWebFavoritesUri()
        {
            return new Uri($"https://ksuite.infomaniak.com/kdrive/app/drive/{Id}/favorites");
        }

        public Uri GetWebSharedUri()
        {
            return new Uri($"https://ksuite.infomaniak.com/kdrive/app/drive/{Id}/shared-with-me");
        }

        public bool HasError
        {
            get => _hasError;
            set => SetProperty(ref _hasError, value);
        }

        public User User
        {
            get => _user;
            set => SetProperty(ref _user, value);
        }

        private void Sync_PropertyChanged(object? sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName == nameof(Sync.SyncErrors))
            {
                HasError = Syncs.Any(s => s.SyncErrors.Count > 0);
            }
        }

        private void RefreshHasError()
        {
            foreach (var sync in Syncs)
            {
                if (sync.SyncErrors.Any())
                {
                    HasError = true;
                    return;
                }
            }
            HasError = false;
        }

        private void InitWatchers()
        {
            _syncs.CollectionChanged += (s, e) =>
            {
                if (e.NewItems != null)
                {
                    foreach (Sync sync in e.NewItems)
                    {
                        sync.PropertyChanged += Sync_PropertyChanged;
                    }
                }
                if (e.OldItems != null)
                {
                    foreach (Sync sync in e.OldItems)
                    {
                        sync.PropertyChanged -= Sync_PropertyChanged;
                    }
                }
                RefreshHasError();
            };
        }
    }
}