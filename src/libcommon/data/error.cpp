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

#include "data/error.h"

#include "libcommon/utility/utility.h"

#include <ctime>
#include <sstream>
#include <chrono>

static const auto outParamsDbId = "dbId";
static const auto outParamsTime = "time";
static const auto outParamsLevel = "level";
static const auto outParamsFunctionName = "functionName";
static const auto outParamsSyncDbId = "syncDbId";
static const auto outParamsWorkerName = "workerName";
static const auto outParamsExitCode = "exitCode";
static const auto outParamsExitCause = "exitCause";
static const auto outParamsLocalNodeId = "localNodeId";
static const auto outParamsRemoteNodeId = "remoteNodeId";
static const auto outParamsNodeType = "nodeType";
static const auto outParamsPath = "path";
static const auto outParamsDestinationPath = "destinationPath";
static const auto outParamsConflictType = "conflictType";
static const auto outParamsInconsistencyType = "inconsistencyType";
static const auto outParamsCancelType = "cancelType";
static const auto outParamsAutoResolved = "autoResolved";

namespace KDC {

Error::Error(const std::string &functionName, const ExitCode exitCode, const ExitCause exitCause) :
    _time(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())),
    _level(ErrorLevel::Server),
    _functionName(functionName),
    _exitCode(exitCode),
    _exitCause(exitCause) {}

Error::Error(const std::string &functionName, const ExitInfo &exitInfo) :
    _time(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())),
    _level(ErrorLevel::Server),
    _functionName(functionName),
    _exitCode(exitInfo.code()),
    _exitCause(exitInfo.cause()) {}

Error::Error(const SyncDbId syncDbId, const std::string &workerName, const ExitCode exitCode, const ExitCause exitCause) :
    _time(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())),
    _level(ErrorLevel::SyncPal),
    _syncDbId(syncDbId),
    _workerName(workerName),
    _exitCode(exitCode),
    _exitCause(exitCause) {}

Error::Error(const SyncDbId syncDbId, const std::string &workerName, const ExitInfo &exitInfo) :
    _time(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())),
    _level(ErrorLevel::SyncPal),
    _syncDbId(syncDbId),
    _workerName(workerName),
    _exitCode(exitInfo.code()),
    _exitCause(exitInfo.cause()) {}

Error::Error(const SyncDbId syncDbId, const NodeId &localNodeId, const NodeId &remoteNodeId, const NodeType nodeType,
             const std::filesystem::path &path, const ConflictType conflictType,
             const InconsistencyType inconsistencyType /*= InconsistencyType::None */,
             const CancelType cancelType /*= CancelType::None*/,
             const std::filesystem::path &destinationPath /*= std::filesystem::path()*/,
             const ExitCode exitCode /*= ExitCode::Unknown*/, const ExitCause exitCause /*= ExitCause::Unknown*/) :
    _time(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())),
    _level(ErrorLevel::Node),
    _syncDbId(syncDbId),
    _exitCode(exitCode),
    _exitCause(exitCause),
    _localNodeId(localNodeId),
    _remoteNodeId(remoteNodeId),
    _nodeType(nodeType),
    _path(path),
    _destinationPath(destinationPath),
    _conflictType(conflictType),
    _inconsistencyType(inconsistencyType),
    _cancelType(cancelType) {}

Error::Error(const ErrorDbId dbId, const int64_t time, const ErrorLevel level, const std::string &functionName,
             const SyncDbId syncDbId, const std::string &workerName, const ExitCode exitCode, const ExitCause exitCause,
             const NodeId &localNodeId, const NodeId &remoteNodeId, const NodeType nodeType, const std::filesystem::path &path,
             const ConflictType conflictType, const InconsistencyType inconsistencyType,
             const CancelType cancelType /*= CancelType::None*/,
             const std::filesystem::path &destinationPath /*= std::filesystem::path()*/) :
    _dbId(dbId),
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
    _destinationPath(destinationPath),
    _conflictType(conflictType),
    _inconsistencyType(inconsistencyType),
    _cancelType(cancelType) {}

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

void Error::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, outParamsDbId, _dbId);
    CommonUtility::writeValueToStruct(dstruct, outParamsTime, _time);
    CommonUtility::writeValueToStruct(dstruct, outParamsLevel, _level);
    CommonUtility::writeValueToStruct(dstruct, outParamsFunctionName, CommonUtility::str2CommString(_functionName));
    CommonUtility::writeValueToStruct(dstruct, outParamsSyncDbId, _syncDbId);
    CommonUtility::writeValueToStruct(dstruct, outParamsWorkerName, CommonUtility::str2CommString(_workerName));
    CommonUtility::writeValueToStruct(dstruct, outParamsExitCode, _exitCode);
    CommonUtility::writeValueToStruct(dstruct, outParamsExitCause, _exitCause);
    CommonUtility::writeValueToStruct(dstruct, outParamsLocalNodeId, _localNodeId);
    CommonUtility::writeValueToStruct(dstruct, outParamsRemoteNodeId, _remoteNodeId);
    CommonUtility::writeValueToStruct(dstruct, outParamsNodeType, _nodeType);
    CommonUtility::writeValueToStruct(dstruct, outParamsPath, CommonUtility::syncPath2CommString(_path));
    CommonUtility::writeValueToStruct(dstruct, outParamsDestinationPath, CommonUtility::syncPath2CommString(_destinationPath));
    CommonUtility::writeValueToStruct(dstruct, outParamsConflictType, _conflictType);
    CommonUtility::writeValueToStruct(dstruct, outParamsInconsistencyType, _inconsistencyType);
    CommonUtility::writeValueToStruct(dstruct, outParamsCancelType, _cancelType);
    CommonUtility::writeValueToStruct(dstruct, outParamsAutoResolved, isAutoResolved());
}

void Error::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    CommonUtility::readValueFromStruct(dstruct, outParamsDbId, _dbId);
    CommonUtility::readValueFromStruct(dstruct, outParamsTime, _time);
    CommonUtility::readValueFromStruct(dstruct, outParamsLevel, _level);

    CommString functionName;
    CommonUtility::readValueFromStruct(dstruct, outParamsFunctionName, functionName);
    _functionName = CommonUtility::commString2Str(functionName);

    CommonUtility::readValueFromStruct(dstruct, outParamsSyncDbId, _syncDbId);

    CommString workerName;
    CommonUtility::readValueFromStruct(dstruct, outParamsWorkerName, workerName);
    _workerName = CommonUtility::commString2Str(workerName);

    CommonUtility::readValueFromStruct(dstruct, outParamsExitCode, _exitCode);
    CommonUtility::readValueFromStruct(dstruct, outParamsExitCause, _exitCause);
    CommonUtility::readValueFromStruct(dstruct, outParamsLocalNodeId, _localNodeId);
    CommonUtility::readValueFromStruct(dstruct, outParamsRemoteNodeId, _remoteNodeId);
    CommonUtility::readValueFromStruct(dstruct, outParamsNodeType, _nodeType);

    CommString path;
    CommonUtility::readValueFromStruct(dstruct, outParamsPath, path);
    _path = CommonUtility::commString2SyncPath(path);

    CommString destinationPath;
    CommonUtility::readValueFromStruct(dstruct, outParamsDestinationPath, destinationPath);
    _destinationPath = CommonUtility::commString2SyncPath(destinationPath);

    CommonUtility::readValueFromStruct(dstruct, outParamsConflictType, _conflictType);
    CommonUtility::readValueFromStruct(dstruct, outParamsInconsistencyType, _inconsistencyType);
    CommonUtility::readValueFromStruct(dstruct, outParamsCancelType, _cancelType);
    // CommonUtility::readValueFromStruct(dstruct, outParamsAutoResolved, _autoResolved);
}

bool Error::isAutoResolved() const {
    bool autoResolved = false;
    if (_level == ErrorLevel::Server) {
        autoResolved = false;
    } else if (_level == ErrorLevel::SyncPal) {
        autoResolved =
                (_exitCode == ExitCode::NetworkError // Sync is paused, and we try to restart it every RESTART_SYNCS_INTERVAL
                 || (_exitCode == ExitCode::BackError // Sync is stopped and a full sync is restarted
                     && _exitCause != ExitCause::DriveAccessError && _exitCause != ExitCause::DriveNotRenew) ||
                 _exitCode == ExitCode::DataError); // Sync is stopped and a full sync is restarted
    } else if (_level == ErrorLevel::Node) {
        autoResolved =
                (_conflictType != ConflictType::None && !isConflictsWithLocalRename(_conflictType)) ||
                (_inconsistencyType != InconsistencyType::None /*&& _inconsistencyType != InconsistencyType::ForbiddenChar*/) ||
                _cancelType != CancelType::None;
    }

    return autoResolved;
}

} // namespace KDC
