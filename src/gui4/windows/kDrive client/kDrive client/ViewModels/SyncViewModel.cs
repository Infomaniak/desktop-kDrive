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

using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace KDriveClient.ViewModels
{
    internal class Sync : ObservableObject
    {
        private int _dbId = -1;
        private int _id = -1;
        private string _localPath = "";
        private string _remotePath = "";
        private bool _supportVfs = false;

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

        public string LocalPath
        {
            get => _localPath;
            set
            { // Ensure the path ends with a directory separator
                if (!string.IsNullOrEmpty(_localPath) && !_localPath.EndsWith(Path.DirectorySeparatorChar.ToString()))
                {
                    _localPath += Path.DirectorySeparatorChar;
                }
                SetProperty(ref _localPath, value);
            }
        }

        public string RemotePath
        {
            get => _remotePath;
            set
            {
                // Ensure the path ends with a directory separator
                if (!string.IsNullOrEmpty(_remotePath) && !_remotePath.EndsWith(Path.DirectorySeparatorChar.ToString()))
                {
                    _remotePath += Path.DirectorySeparatorChar;
                }
                SetProperty(ref _remotePath, value);
            }
        }

        public bool SupportVfs
        {
            get => _supportVfs;
            set => SetProperty(ref _supportVfs, value);
        }
    }
}
