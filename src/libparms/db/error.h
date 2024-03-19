/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#pragma once

#include "parmslib.h"
#include "libcommon/utility/types.h"

#include <string>
#include <filesystem>

namespace KDC {

class PARMS_EXPORT Error {
    public:
        Error();

        // Error level Server constructor
        Error(const std::string &functionName, ExitCode exitCode, ExitCause exitCause);

        // Error level SyncPal constructor
        Error(int syncDbId, const std::string &workerName, ExitCode exitCode, ExitCause exitCause);

        // Error level Node constructor
        Error(int syncDbId, const NodeId &localNodeId, const NodeId &remoteNodeId, NodeType nodeType, const SyncPath &path,
              ConflictType conflictType, InconsistencyType inconsistencyType = InconsistencyTypeNone,
              CancelType cancelType = CancelTypeNone, const SyncPath &destinationPath = "", ExitCode exitCode = ExitCodeUnknown,
              ExitCause exitCause = ExitCauseUnknown);

        Error(int64_t dbId, int64_t time, ErrorLevel level, const std::string &functionName, int syncDbId,
              const std::string &workerName, ExitCode exitCode, ExitCause exitCause, const NodeId &localNodeId,
              const NodeId &remoteNodeId, NodeType nodeType, const SyncPath &path, ConflictType conflictType,
              InconsistencyType inconsistencyType = InconsistencyTypeNone, CancelType cancelType = CancelTypeNone,
              const SyncPath &destinationPath = "");

        inline int64_t dbId() const { return _dbId; }
        inline int64_t time() const { return _time; }
        inline void setTime(int64_t time) { _time = time; }
        inline ErrorLevel level() const { return _level; }
        inline const std::string &functionName() const { return _functionName; }
        inline int syncDbId() const { return _syncDbId; }
        inline const std::string &workerName() const { return _workerName; }
        inline ExitCode exitCode() const { return _exitCode; }
        inline ExitCause exitCause() const { return _exitCause; }
        inline const NodeId &localNodeId() const { return _localNodeId; }
        inline const NodeId &remoteNodeId() const { return _remoteNodeId; }
        inline NodeType nodeType() const { return _nodeType; }
        inline const SyncPath &path() const { return _path; }
        inline const SyncPath &destinationPath() const { return _destinationPath; }
        inline ConflictType conflictType() const { return _conflictType; }
        inline InconsistencyType inconsistencyType() const { return _inconsistencyType; }
        inline CancelType cancelType() const { return _cancelType; }
        inline void setCancelType(CancelType newCancelType) { _cancelType = newCancelType; }

        std::string errorString() const;

    private:
        int64_t _dbId;
        int64_t _time;
        ErrorLevel _level;
        std::string _functionName;
        int _syncDbId;
        std::string _workerName;
        ExitCode _exitCode;
        ExitCause _exitCause;
        NodeId _localNodeId;
        NodeId _remoteNodeId;
        NodeType _nodeType;
        SyncPath _path;
        ConflictType _conflictType;
        InconsistencyType _inconsistencyType;
        CancelType _cancelType;
        SyncPath _destinationPath;
};

}  // namespace KDC
