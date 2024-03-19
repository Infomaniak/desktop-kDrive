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

namespace KDC {

class SyncNode {
    public:
        SyncNode();
        SyncNode(const NodeId &nodeId, SyncNodeType type);

        inline const NodeId &nodeId() const { return _nodeId; }
        inline void setNodeId(const NodeId &newNodeId) { _nodeId = newNodeId; }
        inline SyncNodeType type() const { return _type; }
        inline void setType(SyncNodeType newType) { _type = newType; }

    private:
        NodeId _nodeId;
        SyncNodeType _type;
};

}  // namespace KDC
