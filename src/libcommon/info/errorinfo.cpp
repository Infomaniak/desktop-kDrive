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

#include "errorinfo.h"
#include "utility/utility.h"

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

ErrorInfo::ErrorInfo(qint64 time, ErrorLevel level, const QString &functionName, const SyncDbId syncDbId,
                     const QString &workerName, ExitCode exitCode, ExitCause exitCause, const QString &localNodeId,
                     const QString &remoteNodeId, NodeType nodeType, const QString &path, ConflictType conflictType,
                     InconsistencyType inconsistencyType, CancelType cancelType /*= CancelType::None*/,
                     const QString &destinationPath /*= ""*/) :
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

ErrorInfo::ErrorInfo(const ErrorDbId dbId, qint64 time, ErrorLevel level, const QString &functionName, const SyncDbId syncDbId,
                     const QString &workerName, ExitCode exitCode, ExitCause exitCause, const QString &localNodeId,
                     const QString &remoteNodeId, NodeType nodeType, const QString &path, ConflictType conflictType,
                     InconsistencyType inconsistencyType, CancelType cancelType /*= CancelType::None*/,
                     const QString &destinationPath /*= ""*/) :
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

void ErrorInfo::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, outParamsDbId, _dbId);
    CommonUtility::writeValueToStruct(dstruct, outParamsTime, _time);
    CommonUtility::writeValueToStruct(dstruct, outParamsLevel, _level);
    CommonUtility::writeValueToStruct(dstruct, outParamsFunctionName, _functionName.toStdString());
    CommonUtility::writeValueToStruct(dstruct, outParamsSyncDbId, _syncDbId);
    CommonUtility::writeValueToStruct(dstruct, outParamsWorkerName, _workerName.toStdString());
    CommonUtility::writeValueToStruct(dstruct, outParamsExitCode, _exitCode);
    CommonUtility::writeValueToStruct(dstruct, outParamsExitCause, _exitCause);
    CommonUtility::writeValueToStruct(dstruct, outParamsLocalNodeId, _localNodeId.toStdString());
    CommonUtility::writeValueToStruct(dstruct, outParamsRemoteNodeId, _remoteNodeId.toStdString());
    CommonUtility::writeValueToStruct(dstruct, outParamsNodeType, _nodeType);
    CommonUtility::writeValueToStruct(dstruct, outParamsPath, _path.toStdString());
    CommonUtility::writeValueToStruct(dstruct, outParamsDestinationPath, _destinationPath.toStdString());
    CommonUtility::writeValueToStruct(dstruct, outParamsConflictType, _conflictType);
    CommonUtility::writeValueToStruct(dstruct, outParamsInconsistencyType, _inconsistencyType);
    CommonUtility::writeValueToStruct(dstruct, outParamsCancelType, _cancelType);
    CommonUtility::writeValueToStruct(dstruct, outParamsAutoResolved, _autoResolved);
}

QDataStream &operator>>(QDataStream &in, ErrorInfo &errorInfo) {
    qint64 dbId = 0;
    qint64 syncDbId = 0;
    in >> dbId >> errorInfo._time >> errorInfo._level >> errorInfo._functionName >> syncDbId >> errorInfo._workerName >>
            errorInfo._exitCode >> errorInfo._exitCause >> errorInfo._localNodeId >> errorInfo._remoteNodeId >>
            errorInfo._nodeType >> errorInfo._path >> errorInfo._destinationPath >> errorInfo._conflictType >>
            errorInfo._inconsistencyType >> errorInfo._cancelType >> errorInfo._autoResolved;
    errorInfo._dbId = static_cast<ErrorDbId>(dbId);
    errorInfo._syncDbId = static_cast<SyncDbId>(syncDbId);
    return in;
}

QDataStream &operator<<(QDataStream &out, const ErrorInfo &errorInfo) {
    const qint64 dbId = static_cast<qint64>(errorInfo._dbId);
    const qint64 syncDbId = static_cast<qint64>(errorInfo._syncDbId);
    out << dbId << errorInfo._time << errorInfo._level << errorInfo._functionName << syncDbId << errorInfo._workerName
        << errorInfo._exitCode << errorInfo._exitCause << errorInfo._localNodeId << errorInfo._remoteNodeId << errorInfo._nodeType
        << errorInfo._path << errorInfo._destinationPath << errorInfo._conflictType << errorInfo._inconsistencyType
        << errorInfo._cancelType << errorInfo._autoResolved;
    return out;
}

QDataStream &operator>>(QDataStream &in, QList<ErrorInfo> &list) {
    int count = 0;
    in >> count;
    for (int i = 0; i < count; i++) {
        ErrorInfo errorInfo;
        in >> errorInfo;
        list.push_back(errorInfo);
    }
    return in;
}

QDataStream &operator<<(QDataStream &out, const QList<ErrorInfo> &list) {
    int count = static_cast<int>(list.size());
    out << count;
    for (int i = 0; i < count; i++) {
        ErrorInfo errorInfo = list[i];
        out << errorInfo;
    }
    return out;
}

} // namespace KDC
