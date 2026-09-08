/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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
