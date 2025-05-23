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

#include "update_detection/update_detector/node.h"
#include "update_detection/update_detector/updatetree.h"

#include <queue>

namespace KDC {

class Conflict;
class ConflictCmp;

class Conflict {
    public:
        Conflict() = default;
        Conflict(std::shared_ptr<Node> node, std::shared_ptr<Node> otherNode, ConflictType type);
        ~Conflict();

        [[nodiscard]] std::shared_ptr<Node> node() const { return _node; }
        [[nodiscard]] std::shared_ptr<Node> otherNode() const { return _otherNode; }
        [[nodiscard]] ConflictType type() const { return _type; }

        [[nodiscard]] ReplicaSide sideOfEvent(OperationType opType) const;
        [[nodiscard]] std::shared_ptr<Node> localNode() const;
        [[nodiscard]] std::shared_ptr<Node> remoteNode() const;

    private:
        std::shared_ptr<Node> _node;
        std::shared_ptr<Node> _otherNode;
        ConflictType _type = ConflictType::None;
};

class ConflictCmp {
    public:
        ConflictCmp(std::shared_ptr<UpdateTree> localUpdateTree, std::shared_ptr<UpdateTree> remoteUpdateTree);
        ~ConflictCmp();

        bool operator()(const Conflict &c1, const Conflict &c2) const;

        [[nodiscard]] SyncPath pathOfEvent(const Conflict &conflict, OperationType optype) const;

    private:
        std::shared_ptr<UpdateTree> _localUpdateTree;
        std::shared_ptr<UpdateTree> _remoteUpdateTree;
};

class ConflictQueue : public SharedObject {
    public:
        ConflictQueue(std::shared_ptr<UpdateTree> localUpdateTree, std::shared_ptr<UpdateTree> remoteUpdateTree);
        ~ConflictQueue();

        void push(Conflict c);
        void pop() const { _queue->pop(); }
        [[nodiscard]] const Conflict &top() const { return _queue->top(); }
        [[nodiscard]] size_t size() const { return _queue->size(); }
        [[nodiscard]] bool empty() const { return _queue->empty(); }
        void clear();
        void initQueue();
        [[nodiscard]] bool hasConflict(const ConflictType t) const { return _existingConflictTypes.contains(t); }

    private:
        ConflictCmp _conflictCmp;
        std::unique_ptr<std::priority_queue<Conflict, std::vector<Conflict>, ConflictCmp>> _queue;
        std::unordered_set<ConflictType> _existingConflictTypes;
};


} // namespace KDC
