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

#include "syncoperation.h"

namespace KDC {

UniqueId SyncOperation::_nextId = 0;

SyncOperation::SyncOperation() : _id(_nextId++) {}

SyncName SyncOperation::nodeName(ReplicaSide side) const {
    auto node = affectedNode()->side() == side ? affectedNode() : correspondingNode();
    if (!node) return {};
    return node->name();
}

SyncPath SyncOperation::nodePath(const ReplicaSide side) const {
    if (!correspondingNode()) return affectedNode()->getPath(); // If there is no corresponding node, ignore side.
    auto node = affectedNode()->side() == side ? affectedNode() : correspondingNode();
    return node->getPath();
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

SyncOpPtr SyncOperationList::getOp(UniqueId id) {
    auto opIt = _allOps.find(id);
    return opIt == _allOps.end() ? nullptr : opIt->second;
}

void SyncOperationList::pushOp(SyncOpPtr op) {
    _allOps.insert({op->id(), op});
    _opSortedList.push_back(op->id());
    _opListByType[op->type()].insert(op->id());
    _node2op[*op->affectedNode()->id()].push_back(op->id());
}

void SyncOperationList::insertOp(std::list<UniqueId>::const_iterator pos, SyncOpPtr op) {
    _allOps.insert({op->id(), op});
    _opSortedList.insert(pos, op->id());
    _opListByType[op->type()].insert(op->id());
    _node2op[*op->affectedNode()->id()].push_back(op->id());
}

void SyncOperationList::deleteOp(std::list<UniqueId>::const_iterator it) {
    UniqueId opId = *it;
    SyncOpPtr syncOp = getOp(opId);
    if (syncOp != nullptr) {
        OperationType type = syncOp->type();
        if (_opListByType[type].contains(opId)) {
            _opListByType[type].erase(opId);
        }
        if (syncOp->affectedNode()) {
            auto nodeId = syncOp->affectedNode()->id();
            if (nodeId.has_value() && _node2op.contains(*nodeId)) {
                _node2op.erase(*nodeId);
            }
        }
    }
    if (_allOps.contains(opId)) {
        _allOps.erase(opId);
    }
    _opSortedList.erase(it);
}

void SyncOperationList::clear() {
    std::unordered_map<UniqueId, SyncOpPtr>::iterator it = _allOps.begin();
    while (it != _allOps.end()) {
        it->second.reset();
        it++;
    }
    _allOps.clear();
    _opSortedList.clear();
    _opListByType.clear();
    _node2op.clear();
}

void SyncOperationList::operator=(const SyncOperationList &other) {
    this->_allOps = other._allOps;
    this->_opSortedList = other._opSortedList;
    this->_opListByType = other._opListByType;
    this->_node2op = other._node2op;
}

void SyncOperationList::getMapIndexToOp(std::unordered_map<UniqueId, int> &map,
                                        OperationType typeFilter /*= OperationType::Unknown*/) {
    int index = 0;
    for (const auto &opId: _opSortedList) {
        SyncOpPtr syncOp = getOp(opId);
        if (syncOp != nullptr) {
            if (typeFilter == OperationType::None) {
                map.insert({syncOp->id(), index++});
            } else {
                if (syncOp->type() == typeFilter) {
                    map.insert({syncOp->id(), index++});
                }
            }
        }
    }
}

} // namespace KDC
