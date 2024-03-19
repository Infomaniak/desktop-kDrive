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

#include <optional>

namespace KDC {

class SyncFileItem {
    public:
        SyncFileItem();
        SyncFileItem(NodeType type, const SyncPath &path, const std::optional<SyncPath> &newPath,
                     const std::optional<NodeId> &localNodeId, const std::optional<NodeId> &remoteNodeId, SyncDirection direction,
                     SyncFileInstruction instruction, SyncFileStatus status, ConflictType conflict,
                     InconsistencyType inconsistency, CancelType cancelType, int64_t size, bool dehydrated);
        SyncFileItem(NodeType type, const SyncPath &path, const std::optional<NodeId> &localNodeId,
                     const std::optional<NodeId> &remoteNodeId, SyncDirection direction, SyncFileInstruction instruction,
                     ConflictType conflict, int64_t size);

        inline NodeType type() const { return _type; }
        inline void setType(NodeType newType) { _type = newType; }
        inline const SyncPath &path() const { return _path; }
        inline void setPath(const SyncPath &newPath) { _path = newPath; }
        inline std::optional<SyncPath> newPath() const { return _newPath; }
        inline void setNewPath(std::optional<SyncPath> newNewPath) { _newPath = newNewPath; }
        inline std::optional<NodeId> localNodeId() const { return _localNodeId; }
        inline void setLocalNodeId(std::optional<NodeId> newLocalNodeId) { _localNodeId = newLocalNodeId; }
        inline std::optional<NodeId> remoteNodeId() const { return _remoteNodeId; }
        inline void setRemoteNodeId(std::optional<NodeId> newRemoteNodeId) { _remoteNodeId = newRemoteNodeId; }
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
        inline std::string error() const { return _error; }
        inline void setError(const std::string &error) { _error = error; }
        inline int64_t size() const { return _size; }
        inline void setSize(int64_t newSize) { _size = newSize; }
        inline time_t modTime() const { return _modTime; }
        inline void setModTime(time_t newModTime) { _modTime = newModTime; }
        inline time_t creationTime() const { return _creationTime; }
        inline void setCreationTime(time_t newCreationTime) { _creationTime = newCreationTime; }
        inline bool dehydrated() const { return _dehydrated; }
        inline void setDehydrated(bool newDehydrated) { _dehydrated = newDehydrated; }
        inline bool confirmed() const { return _confirmed; }
        inline void setConfirmed(bool newConfirmed) { _confirmed = newConfirmed; }
        inline SyncTime timestamp() const { return _timestamp; }
        inline void setTimestamp(SyncTime newTimestamp) { _timestamp = newTimestamp; }

        inline bool isDirectory() const { return _type == NodeTypeDirectory; }

    private:
        NodeType _type;
        SyncPath _path;  // Sync folder relative filesystem path
        std::optional<SyncPath> _newPath;
        std::optional<NodeId> _localNodeId;
        std::optional<NodeId> _remoteNodeId;
        SyncDirection _direction;
        SyncFileInstruction _instruction;
        SyncFileStatus _status;
        ConflictType _conflict;
        InconsistencyType _inconsistency;
        CancelType _cancelType;
        std::string _error;
        int64_t _size;
        time_t _modTime;
        time_t _creationTime;
        bool _dehydrated;
        bool _confirmed;
        SyncTime _timestamp;
};

}  // namespace KDC
