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

#include "syncfileitem.h"

#include <ctime>

namespace KDC {

SyncFileItem::SyncFileItem()
    : _type(NodeTypeUnknown),
      _path(SyncPath()),
      _newPath(std::nullopt),
      _localNodeId(std::nullopt),
      _remoteNodeId(std::nullopt),
      _direction(SyncDirectionUnknown),
      _instruction(SyncFileInstructionNone),
      _status(SyncFileStatusUnknown),
      _conflict(ConflictTypeNone),
      _inconsistency(InconsistencyTypeNone),
      _size(0),
      _dehydrated(false),
      _confirmed(false),
      _timestamp(std::time(0)) {}

SyncFileItem::SyncFileItem(NodeType type, const SyncPath &path, const std::optional<SyncPath> &newPath,
                           const std::optional<NodeId> &localNodeId, const std::optional<NodeId> &remoteNodeId,
                           SyncDirection direction, SyncFileInstruction instruction, SyncFileStatus status, ConflictType conflict,
                           InconsistencyType inconsistency, CancelType cancelType, int64_t size, bool dehydrated)
    : _type(type),
      _path(path),
      _newPath(newPath),
      _localNodeId(localNodeId),
      _remoteNodeId(remoteNodeId),
      _direction(direction),
      _instruction(instruction),
      _status(status),
      _conflict(conflict),
      _inconsistency(inconsistency),
      _cancelType(cancelType),
      _size(size),
      _dehydrated(dehydrated),
      _confirmed(false),
      _timestamp(std::time(0)) {}

SyncFileItem::SyncFileItem(NodeType type, const SyncPath &path, const std::optional<NodeId> &localNodeId,
                           const std::optional<NodeId> &remoteNodeId, SyncDirection direction, SyncFileInstruction instruction,
                           ConflictType conflict, int64_t size)
    : _type(type),
      _path(path),
      _newPath(std::nullopt),
      _localNodeId(localNodeId),
      _remoteNodeId(remoteNodeId),
      _direction(direction),
      _instruction(instruction),
      _status(SyncFileStatusUnknown),
      _conflict(conflict),
      _inconsistency(InconsistencyTypeNone),
      _cancelType(CancelTypeNone),
      _size(size),
      _dehydrated(false),
      _confirmed(false),
      _timestamp(std::time(0)) {}

}  // namespace KDC
