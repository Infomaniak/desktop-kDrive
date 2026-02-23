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

using Infomaniak.kDrive.Types;
using System;

namespace Infomaniak.kDrive.ViewModels
{
    public class Error : UISafeObservableObject
    {
        private DbId _DbId = -1;
        private DateTime _timestamp = DateTime.MinValue;
        private ErrorLevel _errorLevel = ErrorLevel.Unknown;
        private Sync? _sync;
        private ExitCode _exitCode = ExitCode.Unknown;
        private ExitCause _exitCause = ExitCause.Unknown;
        NodeType _nodeType = NodeType.Unknown;
        private string _path = "";
        private string _destinationPath = "";
        private ConflictType _conflictType = ConflictType.None;
        private InconsistencyType _inconsistencyType = InconsistencyType.None;
        private CancelType _cancelType = CancelType.None;
        bool _autoResolved = false;

        public DbId DbId
        {
            get => _DbId;
            set => SetPropertyInUIThread(ref _DbId, value);
        }

        public DateTime Timestamp
        {
            get => _timestamp;
            set => SetPropertyInUIThread(ref _timestamp, value);
        }

        public ErrorLevel ErrorLevel
        {
            get => _errorLevel;
            set => SetPropertyInUIThread(ref _errorLevel, value);
        }

        public Sync? Sync
        {
            get => _sync;
            set => SetPropertyInUIThread(ref _sync, value);
        }

        public ExitCode ExitCode
        {
            get => _exitCode;
            set => SetPropertyInUIThread(ref _exitCode, value);
        }

        public ExitCause ExitCause
        {
            get => _exitCause;
            set => SetPropertyInUIThread(ref _exitCause, value);
        }

        public NodeType NodeType
        {
            get => _nodeType;
            set => SetPropertyInUIThread(ref _nodeType, value);
        }

        public string Path
        {
            get => _path;
            set => SetPropertyInUIThread(ref _path, value);
        }

        public string DestinationPath
        {
            get => _destinationPath;
            set => SetPropertyInUIThread(ref _destinationPath, value.Replace('\\', '/'));
        }

        public ConflictType ConflictType
        {
            get => _conflictType;
            set => SetPropertyInUIThread(ref _conflictType, value);
        }

        public InconsistencyType InconsistencyType
        {
            get => _inconsistencyType;
            set => SetPropertyInUIThread(ref _inconsistencyType, value);
        }

        public CancelType CancelType
        {
            get => _cancelType;
            set => SetPropertyInUIThread(ref _cancelType, value);
        }

        public bool AutoResolved
        {
            get => _autoResolved;
            set => SetPropertyInUIThread(ref _autoResolved, value);
        }
    }
}