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

SyncOperation::SyncOperation() :
    _id(_nextId++) {}

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
    const std::scoped_lock lock(_mutex);
    clear();

    for (auto &op: opList) {
        pushOp(op);
    }
}

SyncOpPtr SyncOperationList::getOp(const UniqueId id) const {
    const std::scoped_lock lock(_mutex);
    const auto opIt = _allOps.find(id);
    return opIt == _allOps.end() ? nullptr : opIt->second;
}

const std::list<UniqueId> &SyncOperationList::opSortedList() const {
    return _opSortedList;
}

const std::unordered_set<UniqueId> &SyncOperationList::opListIdByType(const OperationType type) {
    if (const auto it = _opListByType.find(type); it != _opListByType.end()) {
        return it->second;
    }
    static const std::unordered_set<UniqueId> emptySet;
    return emptySet;
}

std::list<UniqueId> SyncOperationList::getOpIdsFromSourceNodeId(const NodeId &nodeId, const ReplicaSide side) {
    const std::scoped_lock lock(_mutex);
    std::list<UniqueId> opList;
    if (const auto it = _nodeIdSource2ops.find(nodeId); it != _nodeIdSource2ops.end()) {
        for (const auto opId: it->second) {
            if (const auto it2 = _allOps.find(opId); it2 != _allOps.end()) {
                const auto opPtr = it2->second;
                // Keep the op only if its source side is the same as `side`.
                if (opPtr && otherSide(opPtr->targetSide()) == side) opList.push_back(opId);
            }
        }
    }

    return opList;
}

SyncOpPtr SyncOperationList::getOpFromTargetNodeId(const NodeId &nodeId, ReplicaSide side, OperationType type,
                                                   const SyncPath &relativePath) {
    const std::scoped_lock lock(_mutex);
    if (const auto it = _nodeIdTarget2ops.find(nodeId); it != _nodeIdTarget2ops.end()) {
        for (const auto opId: it->second) {
            if (const auto it2 = _allOps.find(opId); it2 != _allOps.end()) {
                // Filter by side, type and path
                if (const auto opPtr = it2->second; opPtr && opPtr->targetSide() == side && opPtr->type() == type &&
                                                    opPtr->correspondingNode() &&
                                                    opPtr->correspondingNode()->getPath() == relativePath)
                    return opPtr;
            }
        }
    }
    return nullptr;
}

const std::unordered_map<UniqueId, SyncOpPtr> &SyncOperationList::allOps() const {
    return _allOps;
}

bool SyncOperationList::pushOp(SyncOpPtr op) {
    const std::scoped_lock lock(_mutex);
    const auto [it, inserted] = _allOps.try_emplace(op->id(), op);
    if (!inserted) return false;
    _opSortedList.push_back(op->id());
    (void) _opListByType[op->type()].insert(op->id());
    _nodeIdSource2ops[*op->affectedNode()->id()].push_back(op->id());
    if (op->correspondingNode() && op->correspondingNode()->id().has_value()) {
        _nodeIdTarget2ops[*op->correspondingNode()->id()].push_back(op->id());
    }
    return true;
}

void SyncOperationList::popOp() {
    const std::scoped_lock lock(_mutex);
    const auto opId = _opSortedList.back();
    const auto op = getOp(opId);
    _opSortedList.pop_back();
    _opListByType[op->type()].erase(opId);
    _nodeIdSource2ops[*op->affectedNode()->id()].pop_back();
    if (op->correspondingNode() && op->correspondingNode()->id().has_value()) {
        _nodeIdTarget2ops[*op->correspondingNode()->id()].pop_back();
    }
    _allOps.erase(opId);
}

void SyncOperationList::insertOp(const std::list<UniqueId>::const_iterator pos, SyncOpPtr op) {
    const std::scoped_lock lock(_mutex);
    _allOps.insert({op->id(), op});
    _opSortedList.insert(pos, op->id());
    _opListByType[op->type()].insert(op->id());
    _nodeIdSource2ops[*op->affectedNode()->id()].push_back(op->id());
    if (op->correspondingNode() && op->correspondingNode()->id().has_value()) {
        _nodeIdTarget2ops[*op->correspondingNode()->id()].push_back(op->id());
    }
}

void SyncOperationList::deleteOp(const std::list<UniqueId>::const_iterator it) {
    const std::scoped_lock lock(_mutex);
    const auto opId = *it;
    if (const auto syncOp = getOp(opId); syncOp) {
        const auto type = syncOp->type();
        if (const auto _opListByTypeIt = _opListByType.find(type); _opListByTypeIt != _opListByType.end()) {
            (void) _opListByTypeIt->second.erase(opId);
        }

        if (syncOp->affectedNode()) {
            if (const auto nodeId = syncOp->affectedNode()->id(); nodeId.has_value()) {
                if (const auto nodeIdSource2opsIt = _nodeIdSource2ops.find(*nodeId);
                    nodeIdSource2opsIt != _nodeIdSource2ops.end()) {
                    (void) std::erase_if(nodeIdSource2opsIt->second, [opId](UniqueId value) { return value == opId; });
                    if (nodeIdSource2opsIt->second.empty()) {
                        (void) _nodeIdSource2ops.erase(nodeIdSource2opsIt);
                    }
                }
            }
        }

        if (syncOp->correspondingNode()) {
            if (const auto nodeId = syncOp->correspondingNode()->id(); nodeId.has_value()) {
                if (const auto nodeIdTarget2opsIt = _nodeIdTarget2ops.find(*nodeId);
                    nodeIdTarget2opsIt != _nodeIdTarget2ops.end()) {
                    (void) std::erase_if(nodeIdTarget2opsIt->second, [opId](int value) { return value == opId; });
                    if (nodeIdTarget2opsIt->second.empty()) {
                        (void) _nodeIdTarget2ops.erase(nodeIdTarget2opsIt);
                    }
                }
            }
        }
    }

    (void) _allOps.erase(opId);
    (void) _opSortedList.erase(it);
}

size_t SyncOperationList::size() const {
    const std::scoped_lock lock(_mutex);
    return _allOps.size();
}

bool SyncOperationList::isEmpty() const {
    const std::scoped_lock lock(_mutex);
    return _allOps.empty();
}

void SyncOperationList::clear() {
    const std::scoped_lock lock(_mutex);
    std::unordered_map<UniqueId, SyncOpPtr>::iterator it = _allOps.begin();
    while (it != _allOps.end()) {
        it->second.reset();
        ++it;
    }
    _allOps.clear();
    _opSortedList.clear();
    _opListByType.clear();
    _nodeIdSource2ops.clear();
    _nodeIdTarget2ops.clear();
}

SyncOperationList &SyncOperationList::operator=(const SyncOperationList &other) {
    if (this == &other) {
        return *this;
    }

    const std::scoped_lock lock(_mutex, other._mutex);
    this->_allOps = other._allOps;
    this->_opSortedList = other._opSortedList;
    this->_opListByType = other._opListByType;
    this->_nodeIdSource2ops = other._nodeIdSource2ops;
    this->_nodeIdTarget2ops = other._nodeIdTarget2ops;
    return *this;
}

uint64_t SyncOperationList::countOps(ReplicaSide affectedSide, OperationType operationType) const {
    const std::scoped_lock lock(_mutex);
    uint64_t count = 0;
    for (const auto &opId: _opSortedList) {
        const auto syncOp = getOp(opId);
        if (syncOp && syncOp->affectedNode()->side() == affectedSide && syncOp->type() == operationType) count++;
    }
    return count;
}

bool SyncOperationList::isLocalEditCausedBySync(const NodeId &nodeId, const SyncPath &rootPath, const SyncPath &relativePath,
                                                  SyncTime lastModified, int64_t size) {
    const std::scoped_lock lock(_mutex);
    const auto opPtr = getOpFromTargetNodeId(nodeId, ReplicaSide::Local, OperationType::Edit, relativePath);
    if (opPtr && opPtr->affectedNode()) {
        // Check modification time & size
        const SyncTime lastModifiedLocalRemote =
                opPtr->affectedNode()->modificationTime().has_value() ? *opPtr->affectedNode()->modificationTime() : 0;
        const bool sameModifiedTime = CommonUtility::modificationTimesAreEqual(rootPath, lastModifiedLocalRemote, lastModified);
        const bool sameSize = opPtr->affectedNode()->size() == size;
        if (!sameModifiedTime || !sameSize) {
            return false;
        }

        // Check propagation status
        if (opPtr->propagationStatus() == SyncOperation::PropagationStatus::InProgress ||
            opPtr->propagationStatus() == SyncOperation::PropagationStatus::Propagated) {
            LOGW_DEBUG(Log::instance()->getLogger(),
                       L"Edit propagation operation in progress for " << Utility::formatSyncPath(relativePath));
            return true;
        }
    }

    return false;
}

void SyncOperationList::getOpIdToIndexMap(std::unordered_map<UniqueId, int32_t> &map,
                                          const OperationType typeFilter /*= OperationType::None*/) {
    const std::scoped_lock lock(_mutex);
    int32_t index = 0;
    for (const auto &opId: _opSortedList) {
        const auto syncOp = getOp(opId);
        if (!syncOp) continue;
        if (typeFilter != OperationType::None && syncOp->type() != typeFilter) continue;
        (void) map.insert({syncOp->id(), index++});
    }
}

} // namespace KDC
