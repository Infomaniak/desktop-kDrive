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
using DynamicData.Binding;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using System;
using System.Linq;
using System.Reactive.Linq;

namespace Infomaniak.kDrive.TrayIcon
{
    // This class is responsible for updating the tray icon's tooltip and context menu based on the application's state.
    public partial class TrayIconUpdater : IDisposable
    {
        private readonly TrayIconManager _trayIconManager;
        private readonly AppModel _appModel;
        private readonly IDisposable _subscription;

        public TrayIconUpdater(TrayIconManager trayIconManager, AppModel appModel)
        {
            _trayIconManager = trayIconManager;
            _appModel = appModel;

            // Subscribe to changes in the syncs collection and their statuses
            _subscription = _appModel.AllSyncs
                .ToObservableChangeSet()
                .AutoRefresh(sync => sync.SyncStatus) // react when a sync's Status changes
                .Subscribe(_ => UpdateTrayIcon());
        }

        private void UpdateTrayIcon()
        {

            if (_appModel.AllSyncs.Any(sync => sync.SyncStatus == SyncStatus.Running))
            {
                _trayIconManager.SetIconSync();
                return;
            }

            if (_appModel.AllSyncs.All(sync => sync.SyncStatus == SyncStatus.Paused || sync.SyncStatus == SyncStatus.Stopped || sync.SyncStatus == SyncStatus.Offline))
            {
                _trayIconManager.SetIconPause();
                return;
            }

            _trayIconManager.SetIconOk();
        }

        public void Dispose()
        {
            _subscription.Dispose();
        }
    }
}