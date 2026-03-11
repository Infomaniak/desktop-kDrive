using Infomaniak.kDrive.Types;
using System;

namespace Infomaniak.kDrive.ViewModels
{
    public class SearchItem : UISafeObservableObject
    {
        private NodeId _nodeId;
        private string _name;
        private NodeType _type;
        private string? _path;
        private string? _displayPath;
        private DateTime? _modifiedTime;
        private Int64? _size;
        private bool _isAvailableLocally;

        public SearchItem(NodeId nodeId, string name, NodeType type, string path, string displayPath, DateTime modifiedTime, Int64 size, bool isAvailableLocally)
        {
            _nodeId = nodeId;
            _name = name;
            _type = type;
            _path = path;
            _displayPath = displayPath;
            _modifiedTime = modifiedTime;
            _size = size;
            _isAvailableLocally = isAvailableLocally;
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

        public NodeType Type
        {
            get => _type;
            set => SetPropertyInUIThread(ref _type, value);
        }
        public string? Path
        {
            get => _path;
            set => SetPropertyInUIThread(ref _path, value);
        }
        public string? DisplayPath
        {
            get => _displayPath;
            set => SetPropertyInUIThread(ref _displayPath, value);
        }

        public DateTime? ModifiedTime
        {
            get => _modifiedTime;
            set => SetPropertyInUIThread(ref _modifiedTime, value);
        }

        public Int64? Size
        {
            get => _size;
            set => SetPropertyInUIThread(ref _size, value);
        }

        public bool IsAvailableLocally
        {
            get => _isAvailableLocally;
            set => SetPropertyInUIThread(ref _isAvailableLocally, value);
        }
    }
}
