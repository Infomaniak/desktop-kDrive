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

namespace KDC {

FSOperationSet::~FSOperationSet() {
    clear();
}

bool FSOperationSet::getOp(UniqueId id, FSOpPtr &opPtr, bool lockMutex /*= true*/) {
    if (lockMutex) {
        _mutex.lock();
    }

    auto it = _ops.find(id);
    if (it != _ops.end()) {
        opPtr = it->second;
        if (lockMutex) {
            _mutex.unlock();
        }
        return true;
    }

    if (lockMutex) {
        _mutex.unlock();
    }
    return false;
}

bool FSOperationSet::getOpsByType(const OperationType type, std::unordered_set<UniqueId> &ops) {
    const std::lock_guard<std::mutex> lock(_mutex);
    auto it = _opsByType.find(type);
    if (it != _opsByType.end()) {
        ops = it->second;
        return true;
    }
    return false;
}

bool FSOperationSet::getOpsByNodeId(const NodeId &nodeId, std::unordered_set<UniqueId> &ops) {
    const std::lock_guard<std::mutex> lock(_mutex);
    auto it = _opsByNodeId.find(nodeId);
    if (it != _opsByNodeId.end()) {
        ops = it->second;
        return true;
    }
    return false;
}

uint64_t FSOperationSet::nbOpsByType(const OperationType type) {
    const std::lock_guard<std::mutex> lock(_mutex);
    if (auto it = _opsByType.find(type); it != _opsByType.end()) {
        return it->second.size();
    }
    return 0;
}

void FSOperationSet::clear() {
    std::unordered_map<UniqueId, FSOpPtr>::iterator it = _ops.begin();
    while (it != _ops.end()) {
        it->second.reset();
        it++;
    }
    _ops.clear();
    _opsByType.clear();
    _opsByNodeId.clear();
}

void FSOperationSet::insertOp(FSOpPtr opPtr) {
    const std::lock_guard<std::mutex> lock(_mutex);
    startUpdate();
    _ops.insert({opPtr->id(), opPtr});
    _opsByType[opPtr->operationType()].insert(opPtr->id());
    _opsByNodeId[opPtr->nodeId()].insert(opPtr->id());
}

bool FSOperationSet::removeOp(UniqueId id) {
    const std::lock_guard<std::mutex> lock(_mutex);
    startUpdate();
    auto it = _ops.find(id);
    if (it != _ops.end()) {
        _opsByType[it->second->operationType()].erase(id);
        _opsByNodeId[it->second->nodeId()].erase(id);
        _ops.erase(id);
        return true;
    }
    return false;
}

bool FSOperationSet::removeOp(const NodeId &nodeId, const OperationType opType) {
    FSOpPtr op = nullptr;
    if (findOp(nodeId, opType, op)) {
        return removeOp(op->id());
    }
    return false;
}

bool FSOperationSet::findOp(const NodeId &nodeId, const OperationType opType, FSOpPtr &res) {
    const std::lock_guard<std::mutex> lock(_mutex);
    auto it = _opsByNodeId.find(nodeId);
    if (it != _opsByNodeId.end()) {
        for (auto id : it->second) {
            FSOpPtr opPtr = nullptr;
            if (getOp(id, opPtr, false)) {
                if (opPtr->operationType() == opType) {
                    res = opPtr;
                    return true;
                }
            }
        }
    }
    return false;
}

}  // namespace KDC
