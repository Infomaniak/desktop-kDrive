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

#include "conflict.h"
#include "libcommonserver/utility/utility.h"

namespace KDC {

Conflict::Conflict(std::shared_ptr<Node> localNode, std::shared_ptr<Node> remoteNode, ConflictType type)
    : _node(localNode), _correspondingNode(remoteNode), _type(type) {}

Conflict::~Conflict() {
    _node.reset();
    _correspondingNode.reset();
}

ReplicaSide Conflict::sideOfEvent(OperationType opType) const {
    if (node()->hasChangeEvent(opType)) {
        return node()->side();
    } else if (correspondingNode()->hasChangeEvent(opType)) {
        return correspondingNode()->side();
    } else {
        return ReplicaSide::Unknown;
    }
}

std::shared_ptr<Node> Conflict::localNode() const {
    if (node()->side() == ReplicaSide::Local) {
        return node();
    } else if (correspondingNode()->side() == ReplicaSide::Local) {
        return correspondingNode();
    } else {
        return nullptr;
    }
}

std::shared_ptr<Node> Conflict::remoteNode() const {
    return localNode() == _node ? _correspondingNode : _node;
}

ConflictCmp::ConflictCmp(std::shared_ptr<UpdateTree> localUpdateTree, std::shared_ptr<UpdateTree> remoteUpdateTree)
    : _localUpdateTree(localUpdateTree), _remoteUpdateTree(remoteUpdateTree) {}

ConflictCmp::~ConflictCmp() {
    _localUpdateTree.reset();
    _remoteUpdateTree.reset();
}

bool ConflictCmp::operator()(const Conflict &c1, const Conflict &c2) {
    bool ret = false;

    // Compare conflicts types
    if (c1.type() != c2.type()) {
        ret = (c1.type() > c2.type());
    } else {
        // Compare nodes paths
        std::shared_ptr<Node> localNode;
        SyncPath path1;
        SyncPath path2;
        switch (c1.type()) {
            case ConflictType::MoveParentDelete:
            case ConflictType::CreateParentDelete:
            case ConflictType::MoveDelete:
            case ConflictType::EditDelete:
                // Path of deleted node
                path1 = pathOfEvent(c1, OperationType::Delete);
                path2 = pathOfEvent(c2, OperationType::Delete);
                break;
            case ConflictType::MoveMoveSource:
                // Move origin path of the local node
                localNode = c1.localNode();
                if (localNode && localNode->moveOrigin().has_value()) {
                    path1 = *c1.node()->moveOrigin();
                }
                localNode = c2.localNode();
                if (localNode && localNode->moveOrigin().has_value()) {
                    path2 = *c2.correspondingNode()->moveOrigin();
                }
                break;
            case ConflictType::MoveMoveDest:
            case ConflictType::MoveMoveCycle:
            case ConflictType::CreateCreate:
            case ConflictType::EditEdit:
                // Path of local node
                localNode = c1.localNode();
                if (localNode) {
                    path1 = localNode->getPath();
                }
                localNode = c2.localNode();
                if (localNode) {
                    path2 = localNode->getPath();
                }
                break;
            case ConflictType::MoveCreate:
                // Path of the created node
                path1 = pathOfEvent(c1, OperationType::Create);
                path2 = pathOfEvent(c2, OperationType::Create);
                break;
            default:
                break;
        }

        int pathDepth1 = Utility::pathDepth(path1);
        int pathDepth2 = Utility::pathDepth(path2);
        if (pathDepth1 == pathDepth2) {
            // Compare with lexicographical order
            ret = (path1 > path2);
        } else {
            // Compare with depth level
            ret = (pathDepth1 > pathDepth2);
        }
    }

    return ret;
}

SyncPath ConflictCmp::pathOfEvent(const Conflict &conflict, OperationType optype) const {
    ReplicaSide side = conflict.sideOfEvent(optype);
    SyncPath path = (side == ReplicaSide::Local                 ? conflict.node()->getPath()
                     : side == ReplicaSide::Remote ? conflict.correspondingNode()->getPath()
                                                                : std::filesystem::path());

    return path;
}

ConflictQueue::ConflictQueue(std::shared_ptr<UpdateTree> localUpdateTree, std::shared_ptr<UpdateTree> remoteUpdateTree)
    : _conflictCmp(localUpdateTree, remoteUpdateTree) {
    initQueue();
}

ConflictQueue::~ConflictQueue() {
    clear();
    _queue.reset();
}

void ConflictQueue::push(Conflict c) {
    _queue->push(c);
    _existingConflictTypes.insert(c.type());
}

void ConflictQueue::clear() {
    //_queue.reset(new std::priority_queue<Conflict, std::vector<Conflict>, ConflictCmp>(_conflictCmp));
    while (!_queue->empty()) {
        _queue->pop();
    }
    _existingConflictTypes.clear();
}

void ConflictQueue::initQueue() {
    _queue = std::unique_ptr<std::priority_queue<Conflict, std::vector<Conflict>, ConflictCmp>>(
        new std::priority_queue<Conflict, std::vector<Conflict>, ConflictCmp>(_conflictCmp));
}

}  // namespace KDC
