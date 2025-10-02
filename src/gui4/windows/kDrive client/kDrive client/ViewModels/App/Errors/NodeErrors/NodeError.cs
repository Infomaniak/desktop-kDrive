using Infomaniak.kDrive.Types;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels.Errors
{
    public abstract class NodeError : BaseError
    {
        private SyncPath _nodePath = "";
        private NodeId _localNodeId = -1;
        private NodeId _remoteNodeId = -1;
        private NodeType _nodeType = NodeType.File;
        protected NodeError(DbId dbId) : base(dbId) { }

        public SyncPath NodePath
        {
            get => _nodePath;
            set
            {
                SetProperty(ref _nodePath, value);
                if (_nodeType == NodeType.File)
                {
                    SetProperty(ref _nodeType, Utility.DeduceNodeTypeFromFilePath(_nodePath));
                }
            }
        }

        public NodeId LocalNodeId
        {
            get => _localNodeId;
            set => SetProperty(ref _localNodeId, value);
        }

        public NodeId RemoteNodeId
        {
            get => _remoteNodeId;
            set => SetProperty(ref _remoteNodeId, value);
        }

        public NodeType NodeType
        {
            get => _nodeType;
            set => SetProperty(ref _nodeType, value);
        }

        public override sealed string TitleStr()
        {
            return Path.GetFileName(NodePath);
        }

        public override sealed Uri IconUri()
        {
            Converters.ItemTypeToIconConverter converter = new();
            return (Uri)converter.Convert(NodeType, typeof(Uri), null!, "");
        }
    }
}
