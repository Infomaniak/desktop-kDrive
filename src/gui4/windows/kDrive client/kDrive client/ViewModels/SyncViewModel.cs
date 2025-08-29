using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace kDrive_client.DataModel
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
