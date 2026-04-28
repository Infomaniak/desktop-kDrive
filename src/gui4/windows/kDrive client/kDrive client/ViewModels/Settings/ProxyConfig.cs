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
using Infomaniak.kDrive.Types;

namespace Infomaniak.kDrive.ViewModels
{
    public class ProxyConfig : UISafeObservableObject
    {
        private ProxyType _type = ProxyType.None;
        private string _hostName = "";
        private int _port = 0;
        private bool _needsAuth = false;
        private string _user = "";
        private string _pwd = "";
        public bool _manualConfigurationRequired = false;
        public bool ManualConfigrationRequired
        {
            get => _manualConfigurationRequired;
            set => SetPropertyInUIThread(ref _manualConfigurationRequired, value);
        }

        public ProxyType Type
        {
            get => _type;
            set
            {
                SetPropertyInUIThread(ref _type, value);
                UpdateManualConfigurationRequired();
            }
        }
        public string HostName
        {
            get => _hostName;
            set => SetPropertyInUIThread(ref _hostName, value);
        }
        public int Port
        {
            get => _port;
            set => SetPropertyInUIThread(ref _port, value);
        }
        public bool NeedsAuth
        {
            get => _needsAuth;
            set => SetPropertyInUIThread(ref _needsAuth, value);
        }
        public string User
        {
            get => _user;
            set => SetPropertyInUIThread(ref _user, value);
        }
        public string Pwd
        {
            get => _pwd;
            set => SetPropertyInUIThread(ref _pwd, value);
        }

        private void UpdateManualConfigurationRequired()
        {
            ManualConfigrationRequired = Type == ProxyType.HTTP;
        }

        internal ProxyConfig Clone()
        {
            return new ProxyConfig
            {
                Type = this.Type,
                HostName = this.HostName,
                Port = this.Port,
                NeedsAuth = this.NeedsAuth,
                User = this.User,
                Pwd = this.Pwd
            };
        }
    }
}
