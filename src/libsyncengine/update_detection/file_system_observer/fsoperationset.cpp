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

#include "fsoperationset.h"
#include <cassert>

namespace KDC {

FSOperationSet::~FSOperationSet() {
    clear();
}

bool FSOperationSet::getOp(UniqueId id, FSOpPtr &opPtr) const {
    const std::scoped_lock lock(_mutex);
    if (auto it = _ops.find(id); it != _ops.end()) {
        opPtr = it->second;
        return true;
    }
    return false;
}

void FSOperationSet::getAllOps(std::unordered_map<UniqueId, FSOpPtr> &ops) const {
    const std::scoped_lock lock(_mutex);
    ops = _ops;
}

void FSOperationSet::getOpsByType(const OperationType type, std::unordered_set<UniqueId> &ops) const {
    const std::scoped_lock lock(_mutex);
    if (auto it = _opsByType.find(type); it != _opsByType.end()) {
        ops = it->second;
        return;
    }
    ops.clear();
}

void FSOperationSet::getOpsByNodeId(const NodeId &nodeId, std::unordered_set<UniqueId> &ops) const {
    const std::scoped_lock lock(_mutex);
    if (auto it = _opsByNodeId.find(nodeId); it != _opsByNodeId.end()) {
        ops = it->second;
        return;
    }
    ops.clear();
}

uint64_t FSOperationSet::nbOps() const {
    const std::scoped_lock lock(_mutex);
    return _ops.size();
}

uint64_t FSOperationSet::nbOpsByType(const OperationType type) const {
    const std::scoped_lock lock(_mutex);
    if (auto it = _opsByType.find(type); it != _opsByType.end()) {
        return it->second.size();
    }
    return 0;
}

void FSOperationSet::clear() {
    const std::scoped_lock lock(_mutex);
    auto it = _ops.begin();
    while (it != _ops.end()) {
        it->second.reset();
        it++;
    }
    _ops.clear();
    _opsByType.clear();
    _opsByNodeId.clear();
}

void FSOperationSet::insertOp(FSOpPtr opPtr) {
    const std::scoped_lock lock(_mutex);
    startUpdate();
    _ops.try_emplace(opPtr->id(), opPtr);
    _opsByType[opPtr->operationType()].insert(opPtr->id());
    _opsByNodeId[opPtr->nodeId()].insert(opPtr->id());
}

bool FSOperationSet::removeOp(UniqueId id) {
    const std::scoped_lock lock(_mutex);
    startUpdate();
    if (auto it = _ops.find(id); it != _ops.end()) {
        _opsByType[it->second->operationType()].erase(id);
        if (_opsByType.find(it->second->operationType())->second.empty()) {
            _opsByType.erase(it->second->operationType());
        }
        _opsByNodeId[it->second->nodeId()].erase(id);
        if (_opsByNodeId.find(it->second->nodeId())->second.empty()) {
            _opsByNodeId.erase(it->second->nodeId());
        }
        _ops.erase(id);
        return true;
    }
    return false;
}

bool FSOperationSet::removeOp(const NodeId &nodeId, const OperationType opType) {
    if (FSOpPtr op = nullptr; findOp(nodeId, opType, op)) {
        return removeOp(op->id());
    }
    return false;
}

bool FSOperationSet::findOp(const NodeId &nodeId, const OperationType opType, FSOpPtr &res) const {
    const std::scoped_lock lock(_mutex);
    if (!_opsByNodeId.contains(nodeId)) {
        return false;
    }
    for (auto it = _opsByNodeId.find(nodeId); auto id : it->second) {
        FSOpPtr opPtr = nullptr;
        if (getOp(id, opPtr) && opPtr->operationType() == opType) {
            res = opPtr;
            return true;
        }
    }
    return false;
}

ReplicaSide FSOperationSet::side() const {
    const std::scoped_lock lock(_mutex);
    return _side;
}

FSOperationSet &FSOperationSet::operator=(FSOperationSet &other) {
    const std::scoped_lock lock(_mutex, other._mutex);
    if (this != &other) {
        _ops = other._ops;
        _opsByType = other._opsByType;
        _opsByNodeId = other._opsByNodeId;
    }

    return *this;
}

}  // namespace KDC
