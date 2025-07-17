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

namespace KDC {

ErrorInfo::ErrorInfo() {}

ErrorInfo::ErrorInfo(qint64 time, ErrorLevel level, const QString &functionName, int syncDbId, const QString &workerName,
                     ExitCode exitCode, ExitCause exitCause, const QString &localNodeId, const QString &remoteNodeId,
                     NodeType nodeType, const QString &path, ConflictType conflictType, InconsistencyType inconsistencyType,
                     CancelType cancelType /*= CancelType::None*/, const QString &destinationPath /*= ""*/) :
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

ErrorInfo::ErrorInfo(int64_t dbId, qint64 time, ErrorLevel level, const QString &functionName, int syncDbId,
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

QDataStream &operator>>(QDataStream &in, ErrorInfo &errorInfo) {
    qint64 dbId = 0;
    in >> dbId >> errorInfo._time >> errorInfo._level >> errorInfo._functionName >> errorInfo._syncDbId >>
            errorInfo._workerName >> errorInfo._exitCode >> errorInfo._exitCause >> errorInfo._localNodeId >>
            errorInfo._remoteNodeId >> errorInfo._nodeType >> errorInfo._path >> errorInfo._destinationPath >>
            errorInfo._conflictType >> errorInfo._inconsistencyType >> errorInfo._cancelType >> errorInfo._autoResolved;
    errorInfo._dbId = static_cast<int64_t>(dbId);
    return in;
}

QDataStream &operator<<(QDataStream &out, const ErrorInfo &errorInfo) {
    qint64 dbId = static_cast<qint64>(errorInfo._dbId);
    out << dbId << errorInfo._time << errorInfo._level << errorInfo._functionName << errorInfo._syncDbId << errorInfo._workerName
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
