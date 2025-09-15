using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Threading;

namespace KDrive.ViewModels
{
    enum SyncActivityDirection
    {
        Outgoing,
        Incoming
    }
    enum SyncActivityItemType
    {
        Directory,
        File,
        Doc,
        Pdf,
        image,
        video,
        audio,
        Grid
    }
    internal class SyncActivityViewModel : ObservableObject
    {
        private string _name = "";
        private string _parentFolderPath = "";
        private string _localPath = "";
        private int remoteId = -1;
        private DateTime _activityTime;
        private Timer? _activityTimeUpdateTimer;
        private SyncActivityDirection _direction;
        private Int64 _size;
        private SyncActivityItemType _itemType;

        public string Name { get => _name; }
        public string LocalPath
        {
            get => _localPath;
            set
            {
                SetProperty(ref _localPath, value);
                SetProperty(ref _name, System.IO.Path.GetFileName(value) ?? "");
                SetProperty(ref _parentFolderPath, System.IO.Path.GetDirectoryName(value) ?? "");
                if (_itemType == SyncActivityItemType.File)
                {
                    SetProperty(ref _itemType, deduceSyncActivityItemType());
                }
            }
        }
        public int RemoteId { get => remoteId; set => SetProperty(ref remoteId, value); }
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
        public SyncActivityDirection Direction { get => _direction; set => SetProperty(ref _direction, value); }
        public Int64 Size { get => _size; set => SetProperty(ref _size, value); }
        public SyncActivityItemType ItemType
        {
            get => _itemType;

            set
            {
                if (value == SyncActivityItemType.File)
                {
                    SetProperty(ref _itemType, deduceSyncActivityItemType());
                }
                else
                {
                    SetProperty(ref _itemType, value);
                }
            }
        }
        public string ParentFolderPath { get => _parentFolderPath; }

        private SyncActivityItemType deduceSyncActivityItemType()
        {
            string extension = System.IO.Path.GetExtension(Name).ToLower();
            return extension switch
            {
                ".doc" or ".docx" or ".odt" => SyncActivityItemType.Doc,
                ".pdf" => SyncActivityItemType.Pdf,
                ".png" or ".jpg" or ".jpeg" or ".gif" or ".bmp" or ".tiff" or ".svg" => SyncActivityItemType.image,
                ".mp4" or ".avi" or ".mov" or ".wmv" or ".flv" => SyncActivityItemType.video,
                ".mp3" or ".wav" or ".aac" or ".flac" => SyncActivityItemType.audio,
                ".xls" or ".xlsx" or ".ods" => SyncActivityItemType.Grid,
                _ => SyncActivityItemType.File,
            };

        }
    }
}
