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

#include "error.h"
#include "libcommonserver/utility/utility.h"

#include <ctime>
#include <sstream>
#include <chrono>

namespace KDC {

Error::Error(const std::string &functionName, const ExitCode exitCode, const ExitCause exitCause) :
    _level(ErrorLevel::Server),
    _functionName(functionName),
    _exitCode(exitCode),
    _exitCause(exitCause),
    _time(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())) {}

Error::Error(const std::string &functionName, const ExitInfo &exitInfo) :
    _level(ErrorLevel::Server),
    _functionName(functionName),
    _exitCode(exitInfo.code()),
    _exitCause(exitInfo.cause()),
    _time(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())) {}

Error::Error(const SyncDbId syncDbId, const std::string &workerName, const ExitCode exitCode, const ExitCause exitCause) :
    _level(ErrorLevel::SyncPal),
    _syncDbId(syncDbId),
    _workerName(workerName),
    _exitCode(exitCode),
    _exitCause(exitCause),
    _time(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())) {}

Error::Error(const SyncDbId syncDbId, const std::string &workerName, const ExitInfo &exitInfo) :
    _level(ErrorLevel::SyncPal),
    _syncDbId(syncDbId),
    _workerName(workerName),
    _exitCode(exitInfo.code()),
    _exitCause(exitInfo.cause()),
    _time(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())) {}

Error::Error(const SyncDbId syncDbId, const NodeId &localNodeId, const NodeId &remoteNodeId, const NodeType nodeType,
             const SyncPath &path, const ConflictType conflictType,
             const InconsistencyType inconsistencyType /*= InconsistencyType::None */,
             const CancelType cancelType /*= CancelType::None*/, const SyncPath &destinationPath /*= ""*/,
             const ExitCode exitCode /*= ExitCode::Unknown*/, const ExitCause exitCause /*= ExitCause::Unknown*/) :
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
    _destinationPath(destinationPath),
    _time(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())) {}

Error::Error(const ErrorDbId dbId, const int64_t time, const ErrorLevel level, const std::string &functionName,
             const SyncDbId syncDbId, const std::string &workerName, const ExitCode exitCode, const ExitCause exitCause,
             const NodeId &localNodeId, const NodeId &remoteNodeId, const NodeType nodeType, const SyncPath &path,
             const ConflictType conflictType, const InconsistencyType inconsistencyType,
             const CancelType cancelType /*= CancelType::None*/, const SyncPath &destinationPath /*= ""*/) :
    _dbId(dbId),
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
    _destinationPath(destinationPath),
    _time(time) {}

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
                      << " - cancelType: " << _cancelType << " - ExitCode: " << _exitCode << " - ExitCause: " << _exitCause;
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
            return (_exitCode == other.exitCode()) && (_exitCause == other.exitCause());
        }
        case ErrorLevel::Node: {
            return (_conflictType == other.conflictType()) && (_inconsistencyType == other.inconsistencyType()) &&
                   (_cancelType == other.cancelType()) && (_path == other.path() && _destinationPath == other.destinationPath());
        }
        default:
            return false;
    }
}

} // namespace KDC
