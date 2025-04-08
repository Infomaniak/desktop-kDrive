// Infomaniak kDrive - Desktop
// Copyright (C) 2023-2025 Infomaniak Network SA
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "testsituationgenerator.h"

#include "db/dbnode.h"
#include "syncpal/syncpal.h"
#include "test_utility/testhelpers.h"

#include <Poco/JSON/Parser.h>

namespace KDC {

static const std::string localIdSuffix = "l_";
static const std::string remoteIdSuffix = "r_";

class TestSituationGeneratorException final : public std::runtime_error {
    public:
        explicit TestSituationGeneratorException(const std::string &what) : std::runtime_error(what) {}
};

void TestSituationGenerator::generateInitialSituation(const std::string &jsonInputStr) {
    if (!_syncpal) throw TestSituationGeneratorException("Invalid SyncPal pointer!");

    Poco::JSON::Object::Ptr obj;
    try {
        Poco::JSON::Parser parser;
        obj = parser.parse(jsonInputStr).extract<Poco::JSON::Object::Ptr>();
    } catch (Poco::Exception &) {
        throw TestSituationGeneratorException("Invalid JSON input");
    }

    addItem(obj);

    _syncpal->updateTree(ReplicaSide::Local)->drawUpdateTree();
}

std::shared_ptr<Node> TestSituationGenerator::getNode(const ReplicaSide side, const NodeId &id) const {
    return _syncpal->updateTree(side)->getNodeById(generateId(side, id));
}

bool TestSituationGenerator::getDbNode(const NodeId &id, DbNode &dbNode) const {
    bool found = false;
    if (!_syncpal->syncDb()->node(ReplicaSide::Local, generateId(ReplicaSide::Local, id), dbNode, found)) {
        return false;
    }
    return found;
}

std::shared_ptr<Node> TestSituationGenerator::moveNode(const ReplicaSide side, const NodeId &id, const NodeId &newParentId,
                                                       const SyncName &newName /*= {}*/) const {
    const auto newParentNode = newParentId.empty() ? _syncpal->updateTree(side)->rootNode()
                                                   : _syncpal->updateTree(side)->getNodeById(generateId(side, newParentId));
    const auto node = _syncpal->updateTree(side)->getNodeById(generateId(side, id));

    node->setMoveOriginInfos({node->getPath(), node->parentNode()->id().value()});
    (void) node->parentNode()->deleteChildren(node);
    (void) node->setParentNode(newParentNode);
    (void) newParentNode->insertChildren(node);
    if (!newName.empty()) node->setName(newName);
    node->insertChangeEvent(OperationType::Move);
    return node;
}

std::shared_ptr<Node> TestSituationGenerator::renameNode(const ReplicaSide side, const NodeId &id,
                                                         const SyncName &newName) const {
    const auto node = _syncpal->updateTree(side)->getNodeById(generateId(side, id));
    node->setName(newName);
    node->setMoveOriginInfos({node->getPath(), node->parentNode()->id().value()});
    node->insertChangeEvent(OperationType::Move);
    return node;
}

[[maybe_unused]] std::shared_ptr<Node> TestSituationGenerator::editNode(const ReplicaSide side, const NodeId &id) const {
    static uint64_t editCounter = 0; // Make sure that 2 consecutive edit operations do not generate the same operation.
    const auto node = _syncpal->updateTree(side)->getNodeById(generateId(side, id));
    const auto lastModifiedDate = node->lastmodified().value();
    node->setLastModified(++editCounter + lastModifiedDate);
    node->insertChangeEvent(OperationType::Edit);
    return node;
}

std::shared_ptr<Node> TestSituationGenerator::deleteNode(const ReplicaSide side, const NodeId &id) const {
    const auto node = getNode(side, id);
    node->setChangeEvents(OperationType::Delete);
    for (const auto &[_, child]: node->children()) {
        child->setChangeEvents(OperationType::Delete);
    }
    return node;
}

NodeId TestSituationGenerator::generateId(const ReplicaSide side, const NodeId &id) const {
    if (id.starts_with(localIdSuffix) || id.starts_with(remoteIdSuffix)) return id;
    return side == ReplicaSide::Local ? localIdSuffix + id : remoteIdSuffix + id;
}

void TestSituationGenerator::addItem(Poco::JSON::Object::Ptr obj, const NodeId &parentId /*= {}*/) {
    std::vector<std::string> keys;
    obj->getNames(keys);

    for (const auto &key: keys) {
        addItem(!obj->isObject(key) ? NodeType::File : NodeType::Directory, key, parentId);

        if (obj->isObject(key)) {
            const auto &childObj = obj->getObject(key);
            addItem(childObj, key);
        }
    }
}

void TestSituationGenerator::addItem(const NodeType itemType, const NodeId &id, const NodeId &parentId) const {
    insertInAllSnapshot(itemType, id, parentId);
    const DbNodeId dbNodeId = insertInDb(itemType, id, parentId);
    insertInAllUpdateTrees(itemType, id, parentId, dbNodeId);
}

void TestSituationGenerator::insertInAllSnapshot(const NodeType itemType, const NodeId &id, const NodeId &parentId) const {
    if (id.empty()) return;
    for (const auto side: {ReplicaSide::Local, ReplicaSide::Remote}) {
        const auto size = itemType == NodeType::File ? testhelpers::defaultFileSize : testhelpers::defaultDirSize;
        const auto parentFinalId = parentId.empty() ? "1" : generateId(side, parentId);
        const SnapshotItem item(generateId(side, id), parentFinalId, Str2SyncName(Utility::toUpper(id)), testhelpers::defaultTime,
                                testhelpers::defaultTime, itemType, size, false, true, true);
        (void) _syncpal->snapshot(side)->updateItem(item);
    }
}

DbNodeId TestSituationGenerator::insertInDb(const NodeType itemType, const NodeId &id, const NodeId &parentId) const {
    DbNode parentNode;
    if (parentId.empty()) {
        parentNode = _syncpal->syncDb()->rootNode();
    } else {
        bool found = false;
        if (!_syncpal->syncDb()->node(ReplicaSide::Local, generateId(ReplicaSide::Local, parentId), parentNode, found)) {
            throw TestSituationGeneratorException("Failed to find parent node");
        }
        if (!found) {
            throw TestSituationGeneratorException("Failed to find parent node");
        }
    }

    const auto size = itemType == NodeType::File ? testhelpers::defaultFileSize : testhelpers::defaultDirSize;
    const DbNode dbNode(parentNode.nodeId(), Str2SyncName(Utility::toUpper(id)), Str2SyncName(Utility::toUpper(id)),
                        generateId(ReplicaSide::Local, id), generateId(ReplicaSide::Remote, id), testhelpers::defaultTime,
                        testhelpers::defaultTime, testhelpers::defaultTime, itemType, size);
    DbNodeId dbNodeId = 0;
    bool constraintError = false;
    (void) _syncpal->syncDb()->insertNode(dbNode, dbNodeId, constraintError);
    return dbNodeId;
}

std::shared_ptr<Node> TestSituationGenerator::insertInUpdateTree(
        const ReplicaSide side, const NodeType itemType, const NodeId &id, const NodeId &parentId /*= ""*/,
        const std::optional<DbNodeId> dbNodeId /*= std::nullopt*/) const {
    const auto parentNode = parentId.empty() ? _syncpal->updateTree(side)->rootNode()
                                             : _syncpal->updateTree(side)->getNodeById(generateId(side, parentId));
    const auto size = itemType == NodeType::File ? testhelpers::defaultFileSize : testhelpers::defaultDirSize;
    const auto node =
            std::make_shared<Node>(dbNodeId, side, Str2SyncName(Utility::toUpper(id)), itemType, OperationType::None,
                                   generateId(side, id), testhelpers::defaultTime, testhelpers::defaultTime, size, parentNode);
    _syncpal->updateTree(side)->insertNode(node);
    (void) parentNode->insertChildren(node);
    return node;
}

void TestSituationGenerator::insertInAllUpdateTrees(const NodeType itemType, const NodeId &id, const NodeId &parentId,
                                                    const DbNodeId dbNodeId) const {
    for (const auto side: {ReplicaSide::Local, ReplicaSide::Remote}) {
        (void) insertInUpdateTree(side, itemType, id, parentId, dbNodeId);
    }
}

} // namespace KDC
