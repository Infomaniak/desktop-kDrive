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

#include "dbnode.h"
#include "libcommonserver/utility/utility.h"

namespace KDC {

DbNode::DbNode(int64_t nodeId, std::optional<DbNodeId> parentNodeId, const SyncName &nameLocal, const SyncName &nameRemote,
               const std::optional<NodeId> &nodeIdLocal, const std::optional<NodeId> &nodeIdRemote,
               std::optional<SyncTime> created, std::optional<SyncTime> lastModifiedLocal,
               std::optional<SyncTime> lastModifiedRemote, NodeType type, int64_t size,
               const std::optional<std::string> &checksum, SyncFileStatus status, bool syncing)
    : _nodeId(nodeId),
      _parentNodeId(parentNodeId),
      _nameLocal(nameLocal),
      _nameRemote(nameRemote),
      _nodeIdLocal(nodeIdLocal),
      _nodeIdRemote(nodeIdRemote),
      _created(created),
      _lastModifiedLocal(lastModifiedLocal),
      _lastModifiedRemote(lastModifiedRemote),
      _type(type),
      _size(size),
      _checksum(checksum),
      _status(status),
      _syncing(syncing) {
    assert(nameLocal == Utility::normalizedSyncName(nameLocal));
    assert(nameRemote == Utility::normalizedSyncName(nameRemote));
}

DbNode::DbNode(std::optional<DbNodeId> parentNodeId, const SyncName &nameLocal, const SyncName &nameRemote,
               const std::optional<NodeId> &nodeIdLocal, const std::optional<NodeId> &nodeIdRemote,
               std::optional<SyncTime> created, std::optional<SyncTime> lastModifiedLocal,
               std::optional<SyncTime> lastModifiedRemote, NodeType type, int64_t size,
               const std::optional<std::string> &checksum, SyncFileStatus status, bool syncing)
    : DbNode(0, parentNodeId, nameLocal, nameRemote, nodeIdLocal, nodeIdRemote, created, lastModifiedLocal, lastModifiedRemote,
             type, size, checksum, status, syncing) {}

DbNode::DbNode()
    : _nodeId(0),
      _parentNodeId(0),
      _nameLocal(SyncName()),
      _nameRemote(SyncName()),
      _nodeIdLocal(std::string()),
      _nodeIdRemote(std::string()),
      _created(0),
      _lastModifiedLocal(0),
      _lastModifiedRemote(0),
      _type(NodeType::Unknown),
      _size(0),
      _checksum(std::string()),
      _status(SyncFileStatus::Unknown),
      _syncing(false) {}

void DbNode::setNameLocal(const SyncName &name) {
    assert(name == Utility::normalizedSyncName(name));
    _nameLocal = name;
}

void DbNode::setNameRemote(const SyncName &name) {
    assert(name == Utility::normalizedSyncName(name));
    _nameRemote = name;
}

}  // namespace KDC
