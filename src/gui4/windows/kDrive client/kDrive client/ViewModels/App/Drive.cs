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
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Microsoft.Extensions.DependencyInjection;
using System;
using System.Collections.ObjectModel;
using System.Collections.Specialized;
using System.Drawing;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public class Drive : UISafeObservableObject, IDrive
    {
        // Drive properties
        private DbId _dbId;
        private DriveId _driveId = -1;
        private string _name = "";
        private Color _color = Color.Blue;
        private readonly ObservableCollection<Sync> _syncs = [];
        private Sync? _mainSync;
        private bool _isConfigured = false; // Indicates if at least one sync (which is not an advanced sync) is set up for this drive
        private bool _isAdmin = false; // Indicates if the user is admin of this drive
        private System.Int64 _size = 0;
        private System.Int64 _usedSize = 0;
        private Account _account;
        private bool _packIsFree = false;
        private string _packName = "";
        private string _packDisplayName = "";
        private UInt64 _packId = 0;
        private KSuiteId _kSuiteId = 0; // Not mandatory, only set if the pack is a kSuite pack. This is used to open the kSuite upsell page in the browser.
        private UpsellTarget _packUpsellTarget = UpsellTarget.Unknown;
        private bool _isAsleep = false;
        private bool _isNotRenew = false;
        private bool _isWakingUp = false;
        private bool _isInMaintenance = false;

        // Drive UI properties
        private bool _displayRemoteSpaceWarning = false;
        private readonly ObservableCollection<Sync> _advancedSyncs = [];


        public Drive(DbId dbId, Account account)
        {
            _dbId = dbId;
            _account = account;
            _syncs.CollectionChanged += Syncs_CollectionChanged;

        }

        private void Syncs_CollectionChanged(object? sender, NotifyCollectionChangedEventArgs e)
        {
            RefreshAdvancedSyncsMap();
        }

        public DbId UserDbId { get => _account.User.DbId; }

        private void RefreshAdvancedSyncsMap()
        {
            MainSync = Syncs.FirstOrDefault(s => s.RemotePath == "/" || s.RemotePath == "");

            var advancedSyncs = Syncs.Where(s => s != MainSync);
            foreach (int i in Enumerable.Range(0, _advancedSyncs.Count).Reverse())
            {
                Sync? sync = _advancedSyncs[i];
                if (!advancedSyncs.Contains(sync))
                    _advancedSyncs.Remove(sync);
            }

            foreach (var sync in advancedSyncs)
                if (!_advancedSyncs.Contains(sync))
                    _advancedSyncs.Add(sync);
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
            get => _account.Id;
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
        public System.Int64 Size
        {
            get => _size;
            set => SetPropertyInUIThread(ref _size, value);
        }

        public System.Int64 UsedSize
        {
            get => _usedSize;
            set => SetPropertyInUIThread(ref _usedSize, value);
        }

        public bool PackIsFree
        {
            get => _packIsFree;
            set => SetPropertyInUIThread(ref _packIsFree, value);
        }
        public string PackName
        {
            get => _packName;
            set => SetPropertyInUIThread(ref _packName, value);
        }
        public string PackDisplayName
        {
            get => _packDisplayName;
            set => SetPropertyInUIThread(ref _packDisplayName, value);
        }
        public UInt64 PackId
        {
            get => _packId;
            set => SetPropertyInUIThread(ref _packId, value);
        }
        public KSuiteId KSuiteId
        {
            get => _kSuiteId;
            set => SetPropertyInUIThread(ref _kSuiteId, value);
        }
        public UpsellTarget PackUpsellTarget
        {
            get => _packUpsellTarget;
            set => SetPropertyInUIThread(ref _packUpsellTarget, value);
        }

        public bool IsAsleep
        {
            get => _isAsleep;
            set { if (SetPropertyInUIThread(ref _isAsleep, value)) RefreshSyncsErrorState(); }
        }

        public bool IsNotRenew
        {
            get => _isNotRenew;
            set { if (SetPropertyInUIThread(ref _isNotRenew, value)) RefreshSyncsErrorState(); }
        }

        public bool IsWakingUp
        {
            get => _isWakingUp;
            set { if (SetPropertyInUIThread(ref _isWakingUp, value)) RefreshSyncsErrorState(); }
        }

        public bool IsInMaintenance
        {
            get => _isInMaintenance;
            set { if (SetPropertyInUIThread(ref _isInMaintenance, value)) RefreshSyncsErrorState(); }
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

        public bool DisplayRemoteSpaceWarning
        {
            get => _displayRemoteSpaceWarning;
            set => SetPropertyInUIThread(ref _displayRemoteSpaceWarning, value);
        }

        public Uri GetWebUri() => App.ServiceProvider.GetRequiredService<IUriStore>().WebAppHomeUrl(DriveId);

        public Uri GetWebTrashUri() => App.ServiceProvider.GetRequiredService<IUriStore>().WebAppTrashUrl(DriveId);

        public Uri GetWebFavoritesUri() => App.ServiceProvider.GetRequiredService<IUriStore>().WebAppFavoritesUrl(DriveId);

        public Uri GetWebSharedUri() => App.ServiceProvider.GetRequiredService<IUriStore>().WebAppSharedUrl(DriveId);

        public Account Account
        {
            get => _account;
            set => SetPropertyInUIThread(ref _account, value);
        }

        public void RefreshSyncsErrorState()
        {
            foreach (var sync in _syncs)
                _ = sync.RefreshErrorState();
        }

        public async Task<bool> RemoveSync(Sync sync, CancellationToken cancellationToken)
        {
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            return await commService.RemoveSync(sync.DbId, cancellationToken);
        }
    }
}