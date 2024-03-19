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

#include "libcommon/utility/types.h"

#include <QDataStream>
#include <QString>
#include <QList>

namespace KDC {

class SyncFileItemInfo {
    public:
        SyncFileItemInfo(NodeType type, const QString &path, const QString &newPath, const QString &localNodeId,
                         const QString &remoteNodeId, SyncDirection direction, SyncFileInstruction instruction,
                         SyncFileStatus status, ConflictType conflict, InconsistencyType inconsistency, CancelType cancelType);
        SyncFileItemInfo();

        inline NodeType type() const { return _type; }
        inline void setType(NodeType newType) { _type = newType; }
        inline const QString &path() const { return _path; }
        inline void setPath(const QString &newPath) { _path = newPath; }
        inline QString newPath() const { return _newPath; }
        inline void setNewPath(QString newNewPath) { _newPath = newNewPath; }
        inline const QString &localNodeId() const { return _localNodeId; }
        inline void setLocalNodeId(const QString &newLocalNodeId) { _localNodeId = newLocalNodeId; }
        inline const QString &remoteNodeId() const { return _remoteNodeId; }
        inline void setRemoteNodeId(const QString &newRemoteNodeId) { _remoteNodeId = newRemoteNodeId; }
        inline SyncDirection direction() const { return _direction; }
        inline void setDirection(SyncDirection newDirection) { _direction = newDirection; }
        inline SyncFileInstruction instruction() const { return _instruction; }
        inline void setInstruction(SyncFileInstruction newInstruction) { _instruction = newInstruction; }
        inline SyncFileStatus status() const { return _status; }
        inline void setStatus(SyncFileStatus newStatus) { _status = newStatus; }
        inline ConflictType conflict() const { return _conflict; }
        inline void setConflict(ConflictType newConflict) { _conflict = newConflict; }
        inline InconsistencyType inconsistency() const { return _inconsistency; }
        inline void setInconsistency(InconsistencyType newInconsistency) { _inconsistency = newInconsistency; }
        inline CancelType cancelType() const { return _cancelType; }
        inline void setCancelType(CancelType newCancelType) { _cancelType = newCancelType; }
        inline const QString &error() const { return _error; }
        inline void setError(const QString &newError) { _error = newError; }

        friend QDataStream &operator>>(QDataStream &in, SyncFileItemInfo &info);
        friend QDataStream &operator<<(QDataStream &out, const SyncFileItemInfo &info);

        friend QDataStream &operator>>(QDataStream &in, QList<SyncFileItemInfo> &list);
        friend QDataStream &operator<<(QDataStream &out, const QList<SyncFileItemInfo> &list);


    private:
        NodeType _type;
        QString _path;  // Sync folder relative filesystem path
        QString _newPath;
        QString _localNodeId;
        QString _remoteNodeId;
        SyncDirection _direction;
        SyncFileInstruction _instruction;
        SyncFileStatus _status;
        ConflictType _conflict;
        InconsistencyType _inconsistency;
        CancelType _cancelType;
        QString _error;
};

}  // namespace KDC
