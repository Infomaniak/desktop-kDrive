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

#include <string>
#include <optional>

namespace KDC {

class DbNode {
    public:
        DbNode(DbNodeId nodeId, std::optional<DbNodeId> parentNodeId, const SyncName &nameLocal, const SyncName &nameRemote,
               const std::optional<NodeId> &nodeIdLocal, const std::optional<NodeId> &nodeIdRemote,
               std::optional<SyncTime> created, std::optional<SyncTime> lastModifiedLocal,
               std::optional<SyncTime> lastModifiedRemote, NodeType type, int64_t size,
               const std::optional<std::string> &checksum, SyncFileStatus status = SyncFileStatusUnknown, bool syncing = false);

        DbNode();

        inline DbNodeId nodeId() const { return _nodeId; }
        inline std::optional<DbNodeId> parentNodeId() const { return _parentNodeId; }
        inline const SyncName &nameLocal() const { return _nameLocal; }
        inline const SyncName &nameRemote() const { return _nameRemote; }
        inline const std::optional<NodeId> &nodeIdLocal() const { return _nodeIdLocal; }
        inline const std::optional<NodeId> &nodeIdRemote() const { return _nodeIdRemote; }
        inline std::optional<SyncTime> created() const { return _created; }
        inline std::optional<SyncTime> lastModifiedLocal() const { return _lastModifiedLocal; }
        inline std::optional<SyncTime> lastModifiedRemote() const { return _lastModifiedRemote; }
        inline NodeType type() const { return _type; }
        inline int64_t size() const { return _size; }
        inline const std::optional<std::string> &checksum() const { return _checksum; }
        inline SyncFileStatus status() const { return _status; }
        inline bool syncing() const { return _syncing; }

        inline void setNodeId(DbNodeId nodeId) { _nodeId = nodeId; }
        inline void setParentNodeId(std::optional<DbNodeId> parentNodeId) { _parentNodeId = parentNodeId; }
        virtual void setNameLocal(const SyncName &name);
        virtual void setNameRemote(const SyncName &name);
        inline void setNodeIdLocal(std::optional<NodeId> newNodeIdLocal) { _nodeIdLocal = newNodeIdLocal; }
        inline void setNodeIdRemote(std::optional<NodeId> newNodeIdDrive) { _nodeIdRemote = newNodeIdDrive; }
        inline void setCreated(std::optional<SyncTime> newCreated) { _created = newCreated; }
        inline void setLastModifiedLocal(std::optional<SyncTime> newLastModifiedLocal) {
            _lastModifiedLocal = newLastModifiedLocal;
        }
        inline void setLastModifiedRemote(std::optional<SyncTime> newLastModifiedDrive) {
            _lastModifiedRemote = newLastModifiedDrive;
        }
        inline void setType(NodeType newNodeType) { _type = newNodeType; }
        inline void setSize(int64_t newSize) { _size = newSize; }
        inline void setChecksum(std::optional<std::string> newChecksum) { _checksum = newChecksum; }
        inline void setStatus(SyncFileStatus status) { _status = status; }
        inline void setSyncing(bool syncing) { _syncing = syncing; }

    protected:
        DbNodeId _nodeId;
        std::optional<DbNodeId> _parentNodeId;
        SyncName _nameLocal;   // /!\ Must be in NFC form
        SyncName _nameRemote;  // /!\ Must be in NFC form
        std::optional<NodeId> _nodeIdLocal;
        std::optional<NodeId> _nodeIdRemote;
        std::optional<SyncTime> _created;
        std::optional<SyncTime> _lastModifiedLocal;
        std::optional<SyncTime> _lastModifiedRemote;
        NodeType _type;
        int64_t _size;
        std::optional<std::string> _checksum;
        SyncFileStatus _status;
        bool _syncing;
};

}  // namespace KDC
