using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace kDrive_client.Model
{
    internal class Sync : ObservableObject
    {
        private long _id = -1;
        private string _localPath = "";
        private string _remotePath = "";
        private bool _supportVfs = false;


        public long Id
        {
            get => _id;
            set => SetProperty(ref _id, value);
        }

        public string LocalPath
        {
            get => _localPath;
            set
            {
                if (SetProperty(ref _localPath, value))
                {
                    // Ensure the path ends with a directory separator
                    if (!string.IsNullOrEmpty(_localPath) && !_localPath.EndsWith(Path.DirectorySeparatorChar.ToString()))
                    {
                        _localPath += Path.DirectorySeparatorChar;
                    }
                }
            }
        }

        public string RemotePath
        {
            get => _remotePath;
            set
            {
                if (SetProperty(ref _remotePath, value))
                {
                    // Ensure the path ends with a directory separator
                    if (!string.IsNullOrEmpty(_remotePath) && !_remotePath.EndsWith(Path.DirectorySeparatorChar.ToString()))
                    {
                        _remotePath += Path.DirectorySeparatorChar;
                    }
                }
            }
        }

        public bool SupportVfs
        {
            get => _supportVfs;
            set => SetProperty(ref _supportVfs, value);
        }
    }
}
