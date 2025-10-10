using CommunityToolkit.Mvvm.ComponentModel;
using Infomaniak.kDrive.Types;
using System;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public class SyncActivity : ObservableObject
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
        private SyncActivityState _state = SyncActivityState.InProgress;

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
                    SetProperty(ref _nodeType, Utility.DeduceNodeTypeFromFilePath(LocalPath));
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
                }, null, TimeSpan.FromSeconds(30), TimeSpan.FromMinutes(30));
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
                    SetProperty(ref _nodeType, Utility.DeduceNodeTypeFromFilePath(LocalPath));
                }
                else
                {
                    SetProperty(ref _nodeType, value);
                }
            }
        }
        public SyncPath ParentFolderPath { get => _parentFolderPath; }

        public SyncActivityState State
        {
            get => _state;
            set => SetProperty(ref _state, value);
        }


        private readonly Task _simulationTask;
        public SyncActivity()
        {
            _simulationTask = Simulate();
        }

        ~SyncActivity()
        {
            _simulationTask?.Dispose();
        }
        private async Task Simulate()
        {
            Random random = new();
            await Task.Delay(random.Next(1000, 15000));
            bool success = random.Next(0, 10) > 2;
            AppModel.UIThreadDispatcher.TryEnqueue(() =>
            {
                State = success ? SyncActivityState.Successful : SyncActivityState.Failed;
                ActivityTime = DateTime.Now;
            });
        }
    }
}
