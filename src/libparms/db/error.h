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

#pragma once

#include "libparms/parmslib.h"
#include "libcommon/utility/types.h"

#include <string>

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
              ConflictType conflictType, InconsistencyType inconsistencyType = InconsistencyType::None,
              CancelType cancelType = CancelType::None, const SyncPath &destinationPath = "",
              ExitCode exitCode = ExitCode::Unknown, ExitCause exitCause = ExitCause::Unknown);

        Error(int64_t dbId, int64_t time, ErrorLevel level, const std::string &functionName, int syncDbId,
              const std::string &workerName, ExitCode exitCode, ExitCause exitCause, const NodeId &localNodeId,
              const NodeId &remoteNodeId, NodeType nodeType, const SyncPath &path, ConflictType conflictType,
              InconsistencyType inconsistencyType = InconsistencyType::None, CancelType cancelType = CancelType::None,
              const SyncPath &destinationPath = "");

        inline int64_t dbId() const { return _dbId; }
        inline void setDbId(int64_t val) { _dbId = val; }
        inline int64_t time() const { return _time; }
        inline void setTime(int64_t val) { _time = val; }
        inline ErrorLevel level() const { return _level; }
        inline void setLevel(ErrorLevel val) { _level = val; }
        inline const std::string &functionName() const { return _functionName; }
        inline void setFunctionName(const std::string &val) { _functionName = val; }
        inline int syncDbId() const { return _syncDbId; }
        inline void setSyncDbId(int val) { _syncDbId = val; }
        inline const std::string &workerName() const { return _workerName; }
        inline void setWorkerName(const std::string &val) { _workerName = val; }
        inline ExitCode exitCode() const { return _exitCode; }
        inline void setExitCode(ExitCode val) { _exitCode = val; }
        inline ExitCause exitCause() const { return _exitCause; }
        inline void setExitCause(ExitCause val) { _exitCause = val; }
        inline const NodeId &localNodeId() const { return _localNodeId; }
        inline void setLocalNodeId(const NodeId &val) { _localNodeId = val; }
        inline const NodeId &remoteNodeId() const { return _remoteNodeId; }
        inline void setRemoteNodeId(const NodeId &val) { _remoteNodeId = val; }
        inline NodeType nodeType() const { return _nodeType; }
        inline void setNodeType(NodeType val) { _nodeType = val; }
        inline const SyncPath &path() const { return _path; }
        inline void setPath(const SyncPath &val) { _path = val; }
        inline const SyncPath &destinationPath() const { return _destinationPath; }
        inline void setDestinationPath(const SyncPath &val) { _destinationPath = val; }
        inline ConflictType conflictType() const { return _conflictType; }
        inline void setConflictType(ConflictType val) { _conflictType = val; }
        inline InconsistencyType inconsistencyType() const { return _inconsistencyType; }
        inline void setInconsistencyType(InconsistencyType val) { _inconsistencyType = val; }
        inline CancelType cancelType() const { return _cancelType; }
        inline void setCancelType(CancelType val) { _cancelType = val; }

        std::string errorString() const;
        bool isSimilarTo(const Error &other) const;

    private:
        int64_t _dbId{0};
        int64_t _time{0};
        ErrorLevel _level{ErrorLevel::Unknown};
        std::string _functionName;
        int _syncDbId{0};
        std::string _workerName;
        ExitCode _exitCode{ExitCode::Unknown};
        ExitCause _exitCause{ExitCause::Unknown};
        NodeId _localNodeId;
        NodeId _remoteNodeId;
        NodeType _nodeType{NodeType::Unknown};
        SyncPath _path;
        ConflictType _conflictType{ConflictType::None};
        InconsistencyType _inconsistencyType{InconsistencyType::None};
        CancelType _cancelType{CancelType::None};
        SyncPath _destinationPath;
};

} // namespace KDC
