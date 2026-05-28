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
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Microsoft.Extensions.DependencyInjection;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public class UpdateManager : UISafeObservableObject
    {
        private bool _updateEnabled = false;
        private bool _autoUpdateEnabled = false;
        private DistributionChannel _currentChannel = DistributionChannel.Beta;
        private AppVersion? _availableUpdate;
        private bool _fetchingUpdate = false;
        private bool _showNotification = false;

        public bool UpdateEnabled
        {
            get => _updateEnabled;
            set => SetPropertyInUIThread(ref _updateEnabled, value);
        }

        public bool AutoUpdateEnabled
        {
            get => _autoUpdateEnabled;
            set => SetPropertyInUIThread(ref _autoUpdateEnabled, value);
        }

        public DistributionChannel CurrentChannel
        {
            get => _currentChannel;
            set => SetPropertyInUIThread(ref _currentChannel, value);
        }
        public AppVersion? AvailableUpdate
        {
            get => _availableUpdate;
            set
            {
                SetPropertyInUIThread(ref _availableUpdate, value);
            }
        }

        public bool FetchingUpdate
        {
            get => _fetchingUpdate;
            set => SetPropertyInUIThread(ref _fetchingUpdate, value);
        }

        public bool ShowNotification
        {
            get => _showNotification;
            set => SetPropertyInUIThread(ref _showNotification, value);
        }

        public static async Task<bool> StartUpdate()
        {
            return await App.ServiceProvider.GetRequiredService<IServerCommService>().StartUpdate(CancellationToken.None);
        }

        public async Task<bool> SkipVersion()
        {
            bool res = await App.ServiceProvider.GetRequiredService<IServerCommService>().SkipVersion(CancellationToken.None);
            if (res)
                ShowNotification = false;
            return res;
        }

        public async Task<bool> ChangeChannel(DistributionChannel newChannel)
        {
            var previousChannel = CurrentChannel;
            CurrentChannel = newChannel;

            if (!await App.ServiceProvider.GetRequiredService<IServerCommService>().SaveSettings(CancellationToken.None))
            {
                CurrentChannel = previousChannel;
                return false;
            }
            AvailableUpdate = null;
            ShowNotification = false;
            return true;
        }

        public async Task<bool> ChangeAutoUpdate(bool activated)
        {
            AutoUpdateEnabled = activated;
            if (!await App.ServiceProvider.GetRequiredService<IServerCommService>().SaveSettings(CancellationToken.None))
            {
                AutoUpdateEnabled = !activated;
                return false;
            }
            return true;
        }
    }
}
