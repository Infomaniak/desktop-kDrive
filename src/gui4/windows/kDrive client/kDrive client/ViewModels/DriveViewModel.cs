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
using KDriveClient.ServerCommunication;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Drawing;
using System.Linq;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

namespace KDriveClient.ViewModels
{
    internal class Drive : ObservableObject
    {
        private int _dbId = -1;
        private int _id = -1;
        private string _name = "";
        private Color _color = Color.Blue;
        private long _size = 0;
        private long _usedSize = 0;
        private bool _isActive = true; // Indicates if the user choosed to sync this drive
        private ObservableCollection<Sync> _syncs = new ObservableCollection<Sync>();

        public Drive(int dbId)
        {
            DbId = dbId;
        }
        public async Task Reload()
        {
            Task[] tasks = new Task[]
            {
               CommRequests.GetDriveId(DbId).ContinueWith(t => { if (t.Result != null) Id = t.Result.Value; }),
               CommRequests.GetDriveName(DbId).ContinueWith(t => { if (t.Result != null) Name = t.Result; }),
               CommRequests.GetDriveColor(DbId).ContinueWith(t => { if (t.Result != null) Color = t.Result.Value; }),
               CommRequests.GetDriveSize(DbId).ContinueWith(t => { if (t.Result != null) Size = t.Result.Value; }),
               CommRequests.GetDriveUsedSize(DbId).ContinueWith(t => { if (t.Result != null) UsedSize = t.Result.Value; }),
               CommRequests.GetDriveIsActive(DbId).ContinueWith(t => { if (t.Result != null) IsActive = t.Result.Value; }),
               // TODO: Load syncs
            };
            await Task.WhenAll(tasks).ConfigureAwait(false);
        }
        public int DbId
        {
            get => _dbId;
            set => SetProperty(ref _dbId, value);
        }

        public int Id
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

        public ObservableCollection<Sync> Syncs
        {
            get { return _syncs; }
            set => SetProperty(ref _syncs, value);
        }
    }
}