using CommunityToolkit.Mvvm.ComponentModel;
using KDrive.Types;
using System;
using System.Threading;

namespace KDrive.ViewModels
{
    internal class SyncActivity : ObservableObject
    {
        private string _name = "";
        private SyncPath _parentFolderPath = "";
        private SyncPath _localPath = "";
        private NodeId remoteId = -1;
        private DateTime _activityTime;
        private Timer? _activityTimeUpdateTimer;
        private SyncDirection _direction;
        private Int64 _size;
        private NodeType _nodeType;

        public string Name { get => _name; }
        public SyncPath LocalPath
        {
            get => _localPath;
            set
            {
                SetProperty(ref _localPath, value);
                SetProperty(ref _name, System.IO.Path.GetFileName(value) ?? "");
                SetProperty(ref _parentFolderPath, System.IO.Path.GetDirectoryName(value) ?? "");
                if (_nodeType == NodeType.File)
                {
                    SetProperty(ref _nodeType, deduceSyncActivityNodeType());
                }
            }
        }
        public NodeId RemoteId { get => remoteId; set => SetProperty(ref remoteId, value); }
        public DateTime ActivityTime
        {
            get => _activityTime;
            set
            {
                SetProperty(ref _activityTime, value);
                _activityTimeUpdateTimer?.Dispose();
                _activityTimeUpdateTimer = new Timer(_ =>
                {
                    ViewModels.AppModel.UIThreadDispatcher.TryEnqueue(() =>
                    {
                        OnPropertyChanged(nameof(ActivityTime));
                    });
                }, null, TimeSpan.FromMinutes(1), TimeSpan.FromMinutes(1));
            }
        }
        public SyncDirection Direction { get => _direction; set => SetProperty(ref _direction, value); }
        public Int64 Size { get => _size; set => SetProperty(ref _size, value); }
        public NodeType NodeType
        {
            get => _nodeType;

            set
            {
                if (value == NodeType.File)
                {
                    SetProperty(ref _nodeType, deduceSyncActivityNodeType());
                }
                else
                {
                    SetProperty(ref _nodeType, value);
                }
            }
        }
        public SyncPath ParentFolderPath { get => _parentFolderPath; }

        private NodeType deduceSyncActivityNodeType()
        {
            string extension = System.IO.Path.GetExtension(Name).ToLower();
            return extension switch
            {
                ".doc" or ".docx" or ".odt" => NodeType.Doc,
                ".pdf" => NodeType.Pdf,
                ".png" or ".jpg" or ".jpeg" or ".gif" or ".bmp" or ".tiff" or ".svg" => NodeType.Image,
                ".mp4" or ".avi" or ".mov" or ".wmv" or ".flv" => NodeType.Video,
                ".mp3" or ".wav" or ".aac" or ".flac" => NodeType.Audio,
                ".xls" or ".xlsx" or ".ods" => NodeType.Grid,
                _ => NodeType.File,
            };

        }
    }
}
