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

#include "error.h"

#include <ctime>
#include <sstream>

namespace KDC {

Error::Error()
    : _dbId(0),
      _time(std::time(0)),
      _level(ErrorLevel::Unknown),
      _syncDbId(0),
      _exitCode(ExitCode::Unknown),
      _exitCause(ExitCause::Unknown),
      _nodeType(NodeType::Unknown),
      _conflictType(ConflictType::None),
      _inconsistencyType(InconsistencyType::None),
      _cancelType(CancelType::None) {}

Error::Error(const std::string &functionName, ExitCode exitCode, ExitCause exitCause)
    : _dbId(0),
      _time(std::time(0)),
      _level(ErrorLevel::Server),
      _functionName(functionName),
      _syncDbId(0),
      _workerName(std::string()),
      _exitCode(exitCode),
      _exitCause(exitCause),
      _nodeType(NodeType::Unknown),
      _conflictType(ConflictType::None),
      _inconsistencyType(InconsistencyType::None),
      _cancelType(CancelType::None) {}

Error::Error(int syncDbId, const std::string &workerName, ExitCode exitCode, ExitCause exitCause)
    : _dbId(0),
      _time(std::time(0)),
      _level(ErrorLevel::SyncPal),
      _syncDbId(syncDbId),
      _workerName(workerName),
      _exitCode(exitCode),
      _exitCause(exitCause),
      _nodeType(NodeType::Unknown),
      _conflictType(ConflictType::None),
      _inconsistencyType(InconsistencyType::None),
      _cancelType(CancelType::None) {}

Error::Error(int syncDbId, const NodeId &localNodeId, const NodeId &remoteNodeId, NodeType nodeType, const SyncPath &path,
             ConflictType conflictType, InconsistencyType inconsistencyType /*= InconsistencyType::None */,
             CancelType cancelType /*= CancelType::None*/, const SyncPath &destinationPath /*= ""*/
             ,
             ExitCode exitCode /*= ExitCode::Unknown*/, ExitCause exitCause /*= ExitCause::Unknown*/)
    : _dbId(0),
      _time(std::time(0)),
      _level(ErrorLevel::Node),
      _syncDbId(syncDbId),
      _exitCode(exitCode),
      _exitCause(exitCause),
      _localNodeId(localNodeId),
      _remoteNodeId(remoteNodeId),
      _nodeType(nodeType),
      _path(path),
      _conflictType(conflictType),
      _inconsistencyType(inconsistencyType),
      _cancelType(cancelType),
      _destinationPath(destinationPath) {}

Error::Error(int64_t dbId, int64_t time, ErrorLevel level, const std::string &functionName, int syncDbId,
             const std::string &workerName, ExitCode exitCode, ExitCause exitCause, const NodeId &localNodeId,
             const NodeId &remoteNodeId, NodeType nodeType, const SyncPath &path, ConflictType conflictType,
             InconsistencyType inconsistencyType, CancelType cancelType /*= CancelType::None*/,
             const SyncPath &destinationPath /*= ""*/)
    : _dbId(dbId),
      _time(time),
      _level(level),
      _functionName(functionName),
      _syncDbId(syncDbId),
      _workerName(workerName),
      _exitCode(exitCode),
      _exitCause(exitCause),
      _localNodeId(localNodeId),
      _remoteNodeId(remoteNodeId),
      _nodeType(nodeType),
      _path(path),
      _conflictType(conflictType),
      _inconsistencyType(inconsistencyType),
      _cancelType(cancelType),
      _destinationPath(destinationPath) {}

std::string Error::errorString() const {
    std::ostringstream errStream;

    switch (_level) {
        case ErrorLevel::Server:
            errStream << "Level: Server - function: " << _functionName << " - exitCode: " << _exitCode
                      << " - exitCause: " << _exitCause;
            break;
        case ErrorLevel::SyncPal:
            errStream << "Level: SyncPal - worker: " << _workerName << " - exitCode: " << _exitCode
                      << " - exitCause: " << _exitCause;
            break;
        case ErrorLevel::Node:
            errStream << "Level: SyncPal - conflictType: " << _conflictType << " - inconsistencyType: " << _inconsistencyType
                      << " - cancelType: " << _cancelType;
            break;
        default:
            errStream << "Level: Unknown";
            break;
    }

    return errStream.str();
}

bool Error::isSimilarTo(const Error &other) const {
    switch (_level) {
        case ErrorLevel::Server: {
            return (_exitCode == other.exitCode()) && (_exitCause == other.exitCause()) &&
                   (_functionName == other.functionName());
        }
        case ErrorLevel::SyncPal: {
            return (_exitCode == other.exitCode()) && (_exitCause == other.exitCause()) && (_workerName == other.workerName());
        }
        case ErrorLevel::Node: {
            return (_conflictType == other.conflictType()) && (_inconsistencyType == other.inconsistencyType()) &&
                   (_cancelType == other.cancelType()) && (_path == other.path() && _destinationPath == other.destinationPath());
        }
        default:
            return false;
    }
}

}  // namespace KDC
