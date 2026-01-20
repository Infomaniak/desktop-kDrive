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

using DynamicData;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Microsoft.Extensions.DependencyInjection;
using System;
using System.Collections.ObjectModel;
using System.Drawing;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public class Drive : UISafeObservableObject, IDrive
    {
        private DbId _dbId = -1;
        private DriveId _driveId = -1;
        private string _name = "";
        private Color _color = Color.Blue;
        private bool _isFreeOffer = true; // Indicates if the drive is a free offer
        private ObservableCollection<Sync> _syncs = new ObservableCollection<Sync>();
        private Sync? _mainSync;
        private bool _isConfigured = false; // Indicates if at least one sync (which is not an advanced sync) is set up for this drive
        private bool _isAdmin = false; // Indicates if the user is admin of this drive
        private ObservableCollection<Sync> _advancedSyncs = new ObservableCollection<Sync>();

        private Account _account;
        public Drive(DbId dbId, Account account)
        {
            _dbId = dbId;
            _account = account;
            _syncs.CollectionChanged += (s, e) => RefreshAdvancedSyncsMap();

        }
        public DbId UserDbId { get => _account.User.DbId; }

        private void RefreshAdvancedSyncsMap()
        {
            var advancedSyncs = Syncs.Where(s => s != MainSync);
            foreach (int i in Enumerable.Range(0, _advancedSyncs.Count).Reverse())
            {
                Sync? sync = _advancedSyncs.ElementAt(i);
                if (!advancedSyncs.Contains(sync))
                    _advancedSyncs.Remove(sync);
            }

            foreach (var sync in advancedSyncs)
                if (!_advancedSyncs.Contains(sync))
                    _advancedSyncs.Add(sync);

            MainSync = Syncs.FirstOrDefault(s => s.RemotePath == "/" || s.RemotePath == "");
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

        public AccountId AccountId
        {
            get => _account.AccountId;
        }

        public string AccountName
        {
            get => _account.Name;
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
        public bool IsAdmin
        {
            get => _isAdmin;
            set => SetPropertyInUIThread(ref _isAdmin, value);
        }

        public bool IsFreeOffer
        {
            get => _isFreeOffer;
            set => SetPropertyInUIThread(ref _isFreeOffer, value);
        }

        public ObservableCollection<Sync> Syncs
        {
            get { return _syncs; }
        }

        public Sync? MainSync
        {
            get => _mainSync;
            set
            {
                SetPropertyInUIThread(ref _mainSync, value);
                IsConfigured = value is not null;
            }
        }

        public bool IsConfigured
        {
            get => _isConfigured;
            set => SetPropertyInUIThread(ref _isConfigured, value);
        }

        public ObservableCollection<Sync> AdvancedSyncs
        {
            get => _advancedSyncs;
        }

        public Uri GetWebUri()
        {
            return App.Constants.Drive.kSuiteHomeUrl(DriveId);
        }

        public Uri GetWebTrashUri()
        {
            return App.Constants.Drive.TrashUrl(DriveId);
        }

        public Uri GetWebFavoritesUri()
        {
            return App.Constants.Drive.FavoriteUrl(DriveId);
        }

        public Uri GetWebSharedUri()
        {
            return App.Constants.Drive.SharedUrl(DriveId);
        }

        public Account Account
        {
            get => _account;
            set => SetPropertyInUIThread(ref _account, value);
        }

        public async Task RemoveSync(Sync sync, CancellationToken cancellationToken)
        {
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            await commService.RemoveSync(sync.DbId, cancellationToken);
        }

    }
}