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

#include "fsoperation.h"

#include "utility/utility.h"

namespace KDC {

UniqueId FSOperation::_nextId = 0;

FSOperation::FSOperation(OperationType operationType, const NodeId &nodeId, NodeType objectType, SyncTime createdAt /*= 0*/,
                         SyncTime lastModified /*= 0*/, int64_t size /*= 0*/, const SyncPath &path /*= ""*/,
                         const SyncPath &destinationPath /*= ""*/) :
    _id(_nextId++),
    _operationType(operationType),
    _nodeId(nodeId),
    _objectType(objectType),
    _createdAt(createdAt),
    _lastModified(lastModified),
    _size(size),
    _path(path),
    _destinationPath(destinationPath) {}

} // namespace KDC
