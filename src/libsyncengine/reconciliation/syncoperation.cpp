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

SyncOpPtr SyncOperationList::getOp(const UniqueId id) {
    const std::scoped_lock lock(_mutex);
    const auto opIt = _allOps.find(id);
    return opIt == _allOps.end() ? nullptr : opIt->second;
}

const std::list<UniqueId> &SyncOperationList::opSortedList() const {
    const std::scoped_lock lock(_mutex);
    return _opSortedList;
}

const std::unordered_set<UniqueId> &SyncOperationList::opListIdByType(const OperationType type) {
    const std::scoped_lock lock(_mutex);
    return _opListByType[type];
}

std::list<UniqueId> SyncOperationList::getOpIdsFromSourceNodeId(const NodeId &nodeId, const ReplicaSide side) {
    const std::scoped_lock lock(_mutex);
    std::list<UniqueId> opList;
    for (const auto opId: _nodeIdSource2ops[nodeId]) {
        const auto op = _allOps[opId];
        // Keep the op only if its source side is the same as `side`.
        if (op && otherSide(op->targetSide()) == side) opList.push_back(opId);
    }
    return opList;
}

std::vector<SyncOpPtr> SyncOperationList::getOpsFromTargetNodeId(const NodeId &nodeId, ReplicaSide side, OperationType type,
                                                                 const SyncPath &relativePath) {
    const std::scoped_lock lock(_mutex);
    std::vector<SyncOpPtr> opPtrList;
    for (const auto opId: _nodeIdTarget2ops[nodeId]) {
        const auto opPtr = _allOps[opId];
        // Filter by side, type and path
        if (opPtr && opPtr->targetSide() == side && opPtr->type() == type && opPtr->correspondingNode() &&
            opPtr->correspondingNode()->getPath() == relativePath)
            opPtrList.push_back(opPtr);
    }
    return opPtrList;
}

const std::unordered_map<UniqueId, SyncOpPtr> &SyncOperationList::allOps() const {
    const std::scoped_lock lock(_mutex);
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
    if (const auto syncOp = getOp(opId); !syncOp) {
        const auto type = syncOp->type();
        _opListByType[type].erase(opId);
        if (syncOp->affectedNode()) {
            if (const auto nodeId = syncOp->affectedNode()->id(); nodeId.has_value()) {
                (void) _nodeIdSource2ops.erase(*nodeId);
            }
        }
        if (syncOp->correspondingNode()) {
            if (const auto nodeId = syncOp->correspondingNode()->id(); nodeId.has_value()) {
                (void) _nodeIdTarget2ops.erase(*nodeId);
            }
        }
    }
    _allOps.erase(opId);
    _opSortedList.erase(it);
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
    const std::scoped_lock lock(_mutex);
    this->_allOps = other._allOps;
    this->_opSortedList = other._opSortedList;
    this->_opListByType = other._opListByType;
    this->_nodeIdSource2ops = other._nodeIdSource2ops;
    this->_nodeIdTarget2ops = other._nodeIdTarget2ops;
    return *this;
}

bool SyncOperationList::isLocalEditCausedBySync(const NodeId &nodeId, const SyncPath &rootPath, const SyncPath &relativePath,
                                                  SyncTime lastModified, int64_t size) {
    const std::scoped_lock lock(_mutex);
    const auto opPtrList = getOpsFromTargetNodeId(nodeId, ReplicaSide::Local, OperationType::Edit, relativePath);
    if (!opPtrList.empty()) {
        assert(opPtrList.size() == 1);

        // Check modification time & size
        const SyncTime lastModifiedLocalRemote = opPtrList[0]->affectedNode()->modificationTime().has_value()
                                                         ? *opPtrList[0]->affectedNode()->modificationTime()
                                                         : 0;
        const bool sameModifiedTime = CommonUtility::modificationTimesAreEqual(rootPath, lastModifiedLocalRemote, lastModified);
        const bool sameSize = opPtrList[0]->affectedNode()->size() == size;
        if (!sameModifiedTime || !sameSize) {
            return false;
        }

        // Check propagation status
        if (opPtrList[0]->propagationStatus() == SyncOperation::PropagationStatus::InProgress ||
            opPtrList[0]->propagationStatus() == SyncOperation::PropagationStatus::Propagated) {
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
