using Infomaniak.kDrive.ServerCommunication.CommStruct;
using Infomaniak.kDrive.Types;
using System;

namespace Infomaniak.kDrive.ViewModels
{

    public class SyncFileItem : UISafeObservableObject
    {
        // SyncFileItem properties
        private NodeType _type;
        private string _path = "";
        private string _newPath = "";
        private string _localNodeId = "";
        private string _remoteNodeId = "";
        private SyncDirection _direction;
        private SyncFileInstruction _instruction;
        private SyncFileStatus _status;
        private ConflictType _conflict;
        private InconsistencyType _inconsistency;
        private CancelType _cancelType;
        private Int64 _size = 0;
        private string _error = "";
        private DateTime _timestamp = DateTime.Now;
        private string _localPath = "";
        private string _parentFolderPath = "";
        private int _progressPercent = 0;
        public Int64 _operationId = 0;

        public Sync Sync { get; }


        public SyncFileItem(Sync sync)
        {
            Sync = sync;
        }

        public SyncFileItem(Sync sync, SyncFileItemInfo info)
        {
            Sync = sync;
            _type = info.Type ?? NodeType.File;
            _path = info.Path ?? string.Empty;
            _localPath = System.IO.Path.Combine(Sync.LocalPath, _path.TrimStart(System.IO.Path.DirectorySeparatorChar));
            _parentFolderPath = System.IO.Path.GetDirectoryName(LocalPath) ?? "";
            _newPath = info.NewPath ?? string.Empty;
            _localNodeId = info.LocalNodeId ?? string.Empty;
            _remoteNodeId = info.RemoteNodeId ?? string.Empty;
            _direction = info.Direction ?? SyncDirection.Unknown;
            _instruction = info.Instruction ?? SyncFileInstruction.None;
            _status = info.Status ?? SyncFileStatus.Unknown;
            _conflict = info.Conflict ?? ConflictType.None;
            _inconsistency = info.Inconsistency ?? InconsistencyType.None;
            _cancelType = info.CancelType ?? CancelType.None;
            _size = info.Size ?? 0;
            _error = info.Error ?? string.Empty;
            _progressPercent = info.Progress ?? 0;
            _operationId = info.OperationId ?? 0;
        }

        public NodeType Type
        {
            get => _type;
            set => SetPropertyInUIThread(ref _type, value);
        }

        public string Path
        {
            get => _path;
            set
            {
                SetPropertyInUIThread(ref _path, value);
                SetPropertyInUIThread(ref _localPath, System.IO.Path.Combine(Sync.LocalPath, _path.TrimStart(System.IO.Path.DirectorySeparatorChar)), nameof(LocalPath));
                SetPropertyInUIThread(ref _parentFolderPath, System.IO.Path.GetDirectoryName(LocalPath) ?? "", nameof(ParentFolderPath));
            }
        }
        public string LocalPath
        {
            get => _localPath;
        }
        public string NewPath
        {
            get => _newPath;
            set => SetPropertyInUIThread(ref _newPath, value);
        }
        public string LocalNodeId
        {
            get => _localNodeId;
            set => SetPropertyInUIThread(ref _localNodeId, value);
        }
        public string RemoteNodeId
        {
            get => _remoteNodeId;
            set => SetPropertyInUIThread(ref _remoteNodeId, value);
        }
        public SyncDirection Direction
        {
            get => _direction;
            set => SetPropertyInUIThread(ref _direction, value);
        }

        public SyncFileInstruction Instruction
        {
            get => _instruction;
            set => SetPropertyInUIThread(ref _instruction, value);
        }

        public bool IsDeletion
        {
            get => Instruction == SyncFileInstruction.Remove;
        }

        public SyncFileStatus Status
        {
            get => _status;
            set => SetPropertyInUIThread(ref _status, value);
        }

        public int ProgressPercent
        {
            get => _progressPercent;
            set => SetPropertyInUIThread(ref _progressPercent, value);
        }

        public ConflictType Conflict
        {
            get => _conflict;
            set => SetPropertyInUIThread(ref _conflict, value);
        }
        public InconsistencyType Inconsistency
        {
            get => _inconsistency;
            set => SetPropertyInUIThread(ref _inconsistency, value);
        }
        public CancelType CancelType
        {
            get => _cancelType;
            set => SetPropertyInUIThread(ref _cancelType, value);
        }
        public string Error
        {
            get => _error;
            set => SetPropertyInUIThread(ref _error, value);
        }

        public Int64 Size
        {
            get => _size;
            set => SetPropertyInUIThread(ref _size, value);
        }

        public DateTime Timestamp
        {
            get => _timestamp;
            set => SetPropertyInUIThread(ref _timestamp, value);
        }

        public Int64 OperationId
        {
            get => _operationId;
            set => SetPropertyInUIThread(ref _operationId, value);
        }

        // Calculated properties
        public string ParentFolderPath
        {
            get => _parentFolderPath;
        }
    }
}
