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

using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Microsoft.Extensions.DependencyInjection;
using System.Collections.ObjectModel;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public class NewSync : UISafeObservableObject, ISync
    {
        // Sync properties
        private string _localPath = "";
        private string _remotePath = "";
        private NodeId _remoteNodeId = "";
        private SyncType _syncType = SyncType.Unknown;
        private IDrive? drive;
        private ObservableCollection<NodeId> _excludedNodeIds = new ObservableCollection<NodeId>();

        public NewSync() { }
        public NewSync(NewSync other)
        {
            LocalPath = other.LocalPath;
            RemotePath = other.RemotePath;
            RemoteNodeId = other.RemoteNodeId;
            SyncType = other.SyncType;
            Drive = other.Drive;
            ExcludedNodeIds = new ObservableCollection<NodeId>(other.ExcludedNodeIds);
        }

        public string LocalPath
        {
            get => _localPath;
            set => SetPropertyInUIThread(ref _localPath, value);
        }
        public string RemotePath
        {
            get => _remotePath;
            set => SetPropertyInUIThread(ref _remotePath, value);
        }
        public NodeId RemoteNodeId
        {
            get => _remoteNodeId;
            set => SetPropertyInUIThread(ref _remoteNodeId, value);
        }
        public IDrive? Drive
        {
            get => drive;
            set => SetPropertyInUIThread(ref drive, value);
        }

        public SyncType SyncType
        {
            get => _syncType;
            set => SetPropertyInUIThread(ref _syncType, value);
        }

        public ObservableCollection<NodeId> ExcludedNodeIds
        {
            get => _excludedNodeIds;
            set => SetPropertyInUIThread(ref _excludedNodeIds, value);
        }

        public async Task SelectBestVfsMode()
        {
            var commServices = App.ServiceProvider.GetRequiredService<IServerCommService>();
            bool? CanSupportOnlineMode = await commServices.CanPathSupportLiteSync(LocalPath, CancellationToken.None);
            if (CanSupportOnlineMode is null)
                Logger.Log(Logger.Level.Warning, $"Could not determine if the path '{LocalPath}' supports online mode. Defaulting to offline sync.");

            SyncType = (CanSupportOnlineMode ?? false) ? SyncType.Online : SyncType.Offline;
        }
    }
}
