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
using Infomaniak.kDrive.ServerCommunication.Interfaces;
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
    public class Drive : UISafeObservableObject
    {
        private DbId _dbId = -1;
        private DriveId _driveId = -1;
        private string _name = "";
        private Color _color = Color.Blue;
        private long _size = 0;
        private long _usedSize = 0;
        private bool _isActive = false; // Indicates if the user configured this drive on the current device.
        private bool _isPaidOffer = false; // Indicates if the drive is a paid offer (i.e. myKsuite+/pro +, ...)
        private ObservableCollection<Sync> _syncs = new ObservableCollection<Sync>();
        private bool _hasError = false; // Indicate if any of the syncs has an error
        private Account _account;
        public Drive(DbId dbId, Account account)
        {
            DbId = dbId;
            _account = account;
        }

        public DbId DbId
        {
            get => _dbId;
            set => SetPropertyInUIThread(ref _dbId, value);
        }

        public DriveId DriveId
        {
            get => _driveId;
            set => SetPropertyInUIThread(ref _driveId, value);
        }

        public string Name
        {
            get => _name;
            set => SetPropertyInUIThread(ref _name, value);
        }

        public Color Color
        {
            get => _color;
            set => SetPropertyInUIThread(ref _color, value);
        }
        public long Size
        {
            get => _size;
            set => SetPropertyInUIThread(ref _size, value);
        }

        public long UsedSize
        {
            get => _usedSize;
            set => SetPropertyInUIThread(ref _usedSize, value);
        }

        public bool IsActive
        {
            get => _isActive;
            set
            {
                SetPropertyInUIThread(ref _isActive, value);
            }
        }

        public bool IsPaidOffer
        {
            get => _isPaidOffer;
            set => SetPropertyInUIThread(ref _isPaidOffer, value);
        }

        public ObservableCollection<Sync> Syncs
        {
            get { return _syncs; }
            set => SetPropertyInUIThread(ref _syncs, value);
        }

        public Uri GetWebTrashUri()
        {
            return new Uri($"https://ksuite.infomaniak.com/kdrive/app/drive/{DriveId}/trash");
        }

        public Uri GetWebFavoritesUri()
        {
            return new Uri($"https://ksuite.infomaniak.com/kdrive/app/drive/{DriveId}/favorites");
        }

        public Uri GetWebSharedUri()
        {
            return new Uri($"https://ksuite.infomaniak.com/kdrive/app/drive/{DriveId}/shared-with-me");
        }

        public bool HasError
        {
            get => _hasError;
            set => SetPropertyInUIThread(ref _hasError, value);
        }

        public Account Account
        {
            get => _account;
            set => SetPropertyInUIThread(ref _account, value);
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