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
using Microsoft.Extensions.Logging;
using System;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public class Node : UISafeObservableObject
    {
        private NodeId _nodeId;
        private string _name;
        private Int64 _size = 0;
        private NodeId _parentNodeId;
        private string _path;
        private bool _isLoadingSize = false;
        private DbId _userDbId = -1;
        private DriveId _driveId = -1;

        public Node() { }
        public Node(NodeId nodeId, string name, Int64 size, NodeId parentNodeId, string path, DbId userDbId, DriveId driveId)
        {
            _nodeId = nodeId;
            _name = name;
            _size = size;
            _parentNodeId = parentNodeId;
            _path = path;
            _userDbId = userDbId;
            _driveId = driveId;
        }

        public NodeId NodeId
        {
            get => _nodeId;
            set => SetPropertyInUIThread(ref _nodeId, value);
        }

        public string Name
        {
            get => _name;
            set => SetPropertyInUIThread(ref _name, value);
        }

        public Int64 Size
        {
            get => _size;
            set => SetPropertyInUIThread(ref _size, value);
        }

        public NodeId ParentNodeId
        {
            get => _parentNodeId;
            set => SetPropertyInUIThread(ref _parentNodeId, value);
        }

        public string Path
        {
            get => _path;
            set => SetPropertyInUIThread(ref _path, value.Replace('\\', '/'));
        }
        public bool IsLoadingSize
        {
            get => _isLoadingSize;
            set => SetPropertyInUIThread(ref _isLoadingSize, value);
        }

        public DbId UserDbId
        {
            get => _userDbId;
            set => SetPropertyInUIThread(ref _userDbId, value);
        }
        public DriveId DriveId
        {
            get => _driveId;
            set => SetPropertyInUIThread(ref _driveId, value);
        }

        public async Task LoadSize()
        {
            if (UserDbId == -1 || DriveId == -1 || NodeId == null)
            {
                Logger.Log(Logger.Level.Error, "Cannot load node size: UserDbId, DriveId or NodeId is not set.");
            }
            if (IsLoadingSize)
            {
                return;
            }
            IsLoadingSize = true;
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            var res = await commService.GetFolderSize(_userDbId, _driveId, _nodeId, CancellationToken.None);
            if (res is null || res < 0)
            {
                Logger.Log(Logger.Level.Warning, $"Failed to fecth size for NodeId: {NodeId} ");
            }
            Size = res ?? -1;
            IsLoadingSize = false;
        }
    }
}