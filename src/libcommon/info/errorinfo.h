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

#include "../utility/types.h"

#include <QDataStream>
#include <QString>
#include <QList>

namespace KDC {

class ErrorInfo {
    public:
        ErrorInfo(int64_t dbId, qint64 time, ErrorLevel level, const QString &functionName, int syncDbId,
                  const QString &workerName, ExitCode exitCode, ExitCause exitCause, const QString &localNodeId,
                  const QString &remoteNodeId, NodeType nodeType, const QString &path, ConflictType conflictType,
                  InconsistencyType inconsistencyType, CancelType cancelType = CancelType::None,
                  const QString &destinationPath = "");
        ErrorInfo(qint64 time, ErrorLevel level, const QString &functionName, int syncDbId, const QString &workerName,
                  ExitCode exitCode, ExitCause exitCause, const QString &localNodeId, const QString &remoteNodeId,
                  NodeType nodeType, const QString &path, ConflictType conflictType, InconsistencyType inconsistencyType,
                  CancelType cancelType = CancelType::None, const QString &destinationPath = "");
        ErrorInfo();

        inline int64_t dbId() const { return _dbId; }
        inline void setDbId(int64_t dbId) { _dbId = dbId; }
        inline qint64 getTime() const { return _time; }
        inline void setTime(qint64 time) { _time = time; }
        inline ErrorLevel level() const { return _level; }
        inline void setLevel(ErrorLevel newLevel) { _level = newLevel; }
        inline const QString &functionName() const { return _functionName; }
        inline void setFunctionName(const QString &newFunctionName) { _functionName = newFunctionName; }
        inline int syncDbId() const { return _syncDbId; }
        inline void setSyncDbId(int newSyncDbId) { _syncDbId = newSyncDbId; }
        inline const QString &workerName() const { return _workerName; }
        inline void setWorkerName(const QString &newWorkerName) { _workerName = newWorkerName; }
        inline ExitCode exitCode() const { return _exitCode; }
        inline void setExitCode(ExitCode newExitCode) { _exitCode = newExitCode; }
        inline ExitCause exitCause() const { return _exitCause; }
        inline void setExitCause(ExitCause newExitCause) { _exitCause = newExitCause; }
        inline const QString &localNodeId() const { return _localNodeId; }
        inline void setLocalNodeId(const QString &newLocalNodeId) { _localNodeId = newLocalNodeId; }
        inline const QString &remoteNodeId() const { return _remoteNodeId; }
        inline void setRemoteNodeId(const QString &newRemoteNodeId) { _remoteNodeId = newRemoteNodeId; }
        inline NodeType nodeType() const { return _nodeType; }
        inline void setNodeType(NodeType newNodeType) { _nodeType = newNodeType; }
        inline const QString &path() const { return _path; }
        inline void setPath(const QString &newPath) { _path = newPath; }
        inline const QString &destinationPath() const { return _destinationPath; }
        inline void setDestinationPath(const QString &newPath) { _destinationPath = newPath; }
        inline ConflictType conflictType() const { return _conflictType; }
        inline void setConflictType(ConflictType newConflictType) { _conflictType = newConflictType; }
        inline InconsistencyType inconsistencyType() const { return _inconsistencyType; }
        inline void setInconsistencyType(InconsistencyType newInconsistencyType) { _inconsistencyType = newInconsistencyType; }
        inline CancelType cancelType() const { return _cancelType; }
        inline void setCancelType(CancelType newCancelType) { _cancelType = newCancelType; }
        inline bool autoResolved() const { return _autoResolved; }
        inline void setAutoResolved(bool newAutoResolved) { _autoResolved = newAutoResolved; }

        friend QDataStream &operator>>(QDataStream &in, ErrorInfo &errorInfo);
        friend QDataStream &operator<<(QDataStream &out, const ErrorInfo &errorInfo);

        friend QDataStream &operator>>(QDataStream &in, QList<ErrorInfo> &list);
        friend QDataStream &operator<<(QDataStream &out, const QList<ErrorInfo> &list);

    private:
        int64_t _dbId{0};
        qint64 _time{0};
        ErrorLevel _level{ErrorLevel::Unknown};
        QString _functionName;
        int _syncDbId{0};
        QString _workerName;
        ExitCode _exitCode{ExitCode::Unknown};
        ExitCause _exitCause{ExitCause::Unknown};
        QString _localNodeId;
        QString _remoteNodeId;
        NodeType _nodeType{NodeType::Unknown};
        QString _path;
        QString _destinationPath;
        ConflictType _conflictType{ConflictType::None};
        InconsistencyType _inconsistencyType{InconsistencyType::None};
        CancelType _cancelType{CancelType::None};
        bool _autoResolved{false};
};

} // namespace KDC
