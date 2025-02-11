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

#include "syncoperation.h"

namespace KDC {

UniqueId SyncOperation::_nextId = 1;

SyncOperation::SyncOperation() : _id(_nextId++) {}

SyncName SyncOperation::nodeName(const ReplicaSide side) const {
    const auto node = affectedNode()->side() == side ? affectedNode() : correspondingNode();
    if (!node) return {};
    return node->name();
}

SyncPath SyncOperation::nodePath(const ReplicaSide side) const {
    if (!correspondingNode()) return affectedNode()->getPath(); // If there is no corresponding node, ignore side.
    const auto node = affectedNode()->side() == side ? affectedNode() : correspondingNode();
    return node->getPath();
}

NodeType SyncOperation::nodeType() const noexcept {
    return _affectedNode ? _affectedNode->type() : NodeType::Unknown;
}

bool SyncOperation::operator==(const SyncOperation &other) const {
    return _id == other.id();
}

SyncOperationList::~SyncOperationList() {
    clear();
}

void SyncOperationList::setOpList(const std::list<SyncOpPtr> &opList) {
    clear();

    for (auto &op: opList) {
        pushOp(op);
    }
}

SyncOpPtr SyncOperationList::getOp(const UniqueId id) {
    const auto opIt = _allOps.find(id);
    return opIt == _allOps.end() ? nullptr : opIt->second;
}

void SyncOperationList::pushOp(SyncOpPtr op) {
    _allOps.insert({op->id(), op});
    _opSortedList.push_back(op->id());
    _opListByType[op->type()].insert(op->id());
    _node2op[*op->affectedNode()->id()].push_back(op->id());
}

void SyncOperationList::insertOp(const std::list<UniqueId>::const_iterator pos, SyncOpPtr op) {
    _allOps.insert({op->id(), op});
    _opSortedList.insert(pos, op->id());
    _opListByType[op->type()].insert(op->id());
    _node2op[*op->affectedNode()->id()].push_back(op->id());
}

void SyncOperationList::deleteOp(const std::list<UniqueId>::const_iterator it) {
    const auto opId = *it;
    if (const auto syncOp = getOp(opId); !syncOp) {
        const auto type = syncOp->type();
        _opListByType[type].erase(opId);
        if (syncOp->affectedNode()) {
            if (const auto nodeId = syncOp->affectedNode()->id(); nodeId.has_value()) {
                _node2op.erase(*nodeId);
            }
        }
    }
    _allOps.erase(opId);
    _opSortedList.erase(it);
}

void SyncOperationList::clear() {
    std::unordered_map<UniqueId, SyncOpPtr>::iterator it = _allOps.begin();
    while (it != _allOps.end()) {
        it->second.reset();
        ++it;
    }
    _allOps.clear();
    _opSortedList.clear();
    _opListByType.clear();
    _node2op.clear();
}

SyncOperationList &SyncOperationList::operator=(const SyncOperationList &other) {
    this->_allOps = other._allOps;
    this->_opSortedList = other._opSortedList;
    this->_opListByType = other._opListByType;
    this->_node2op = other._node2op;
    return *this;
}

void SyncOperationList::getOpIdToIndexMap(std::unordered_map<UniqueId, int32_t> &map,
                                          const OperationType typeFilter /*= OperationType::None*/) {
    int32_t index = 0;
    for (const auto &opId: _opSortedList) {
        const auto syncOp = getOp(opId);
        if (!syncOp) continue;
        if (typeFilter != OperationType::None && syncOp->type() != typeFilter) continue;
        map.insert({syncOp->id(), index++});
    }
}

} // namespace KDC
