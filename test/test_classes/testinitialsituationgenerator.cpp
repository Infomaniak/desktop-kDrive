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

#include "testinitialsituationgenerator.h"

#include "db/dbnode.h"
#include "syncpal/syncpal.h"
#include "test_utility/testhelpers.h"

#include <Poco/JSON/Parser.h>

namespace KDC {

static const std::string localIdSuffix = "l_";
static const std::string remoteIdSuffix = "r_";

void TestInitialSituationGenerator::generateInitialSituation(const std::string &jsonInputStr) {
    if (!_syncpal) throw std::runtime_error("Invalid SyncPal pointer!");

    Poco::JSON::Object::Ptr obj;
    try {
        Poco::JSON::Parser parser;
        obj = parser.parse(jsonInputStr).extract<Poco::JSON::Object::Ptr>();
    } catch (Poco::Exception &) {
        throw std::runtime_error("Invalid JSON input");
    }

    addItem(obj);

    _syncpal->updateTree(ReplicaSide::Local)->drawUpdateTree();
}

std::shared_ptr<Node> TestInitialSituationGenerator::getNode(const ReplicaSide side, const NodeId &id) const {
    return _syncpal->updateTree(side)->getNodeById(generateId(side, id));
}

bool TestInitialSituationGenerator::getDbNode(const NodeId &id, DbNode &dbNode) const {
    bool found = false;
    if (!_syncpal->syncDb()->node(ReplicaSide::Local, generateId(ReplicaSide::Local, id), dbNode, found)) {
        return false;
    }
    return found;
}

std::shared_ptr<Node> TestInitialSituationGenerator::insertInUpdateTree(
        const ReplicaSide side, const NodeType itemType, const NodeId &id, const NodeId &parentId /*= ""*/,
        const std::optional<DbNodeId> dbNodeId /*= std::nullopt*/) const {
    const auto parentNode = parentId.empty() ? _syncpal->updateTree(side)->rootNode()
                                             : _syncpal->updateTree(side)->getNodeById(generateId(side, parentId));
    const auto size = itemType == NodeType::File ? testhelpers::defaultFileSize : testhelpers::defaultDirSize;
    const auto node = std::make_shared<Node>(dbNodeId, side, Str2SyncName(Utility::toUpper(id)), itemType, OperationType::None,
                                             generateId(side, id), testhelpers::defaultTime, testhelpers::defaultTime, size,
                                             parentNode);
    _syncpal->updateTree(side)->insertNode(node);
    (void) parentNode->insertChildren(node);
    return node;
}

void TestInitialSituationGenerator::moveNode(const ReplicaSide side, const NodeId &id, const NodeId &newParentRawId) const {
    const auto newParentNode = newParentRawId.empty() ? _syncpal->updateTree(side)->rootNode()
                                                      : _syncpal->updateTree(side)->getNodeById(generateId(side, newParentRawId));
    const auto node = _syncpal->updateTree(side)->getNodeById(generateId(side, id));

    node->setMoveOriginInfos({node->getPath(), newParentNode->id().value()});
    (void) node->parentNode()->deleteChildren(node);
    (void) node->setParentNode(newParentNode);
    (void) newParentNode->insertChildren(node);
}

void TestInitialSituationGenerator::editNode(const ReplicaSide side, const NodeId &id) const {
    const auto node = _syncpal->updateTree(side)->getNodeById(generateId(side, id));
    auto lastModifiedDate = node->lastmodified().value();
    node->setLastModified(++lastModifiedDate);
}

void TestInitialSituationGenerator::removeFromUpdateTree(const ReplicaSide side, const NodeId &id) const {
    (void) _syncpal->updateTree(side)->deleteNode(id);
}

NodeId TestInitialSituationGenerator::generateId(const ReplicaSide side, const NodeId &id) const {
    if (id.starts_with(localIdSuffix) || id.starts_with(remoteIdSuffix)) return id;
    return side == ReplicaSide::Local ? localIdSuffix + id : remoteIdSuffix + id;
}

void TestInitialSituationGenerator::addItem(Poco::JSON::Object::Ptr obj, const NodeId &parentId /*= {}*/) {
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

void TestInitialSituationGenerator::addItem(const NodeType itemType, const NodeId &id, const NodeId &parentId) const {
    const DbNodeId dbNodeId = insertInDb(itemType, id, parentId);
    insertInAllUpdateTrees(itemType, id, parentId, dbNodeId);
}


DbNodeId TestInitialSituationGenerator::insertInDb(const NodeType itemType, const NodeId &id, const NodeId &parentId) const {
    DbNode parentNode;
    if (parentId.empty()) {
        parentNode = _syncpal->syncDb()->rootNode();
    } else {
        bool found = false;
        if (!_syncpal->syncDb()->node(ReplicaSide::Local, generateId(ReplicaSide::Local, parentId), parentNode, found)) {
            throw std::runtime_error("Failed to find parent node");
        }
        if (!found) {
            throw std::runtime_error("Failed to find parent node");
        }
    }

    const auto size = itemType == NodeType::File ? testhelpers::defaultFileSize : testhelpers::defaultDirSize;
    const DbNode dbNode(parentNode.nodeId(), Str2SyncName(Utility::toUpper(id)), Str2SyncName(Utility::toUpper(id)),
                        generateId(ReplicaSide::Local, id), generateId(ReplicaSide::Remote, id), testhelpers::defaultTime,
                        testhelpers::defaultTime, testhelpers::defaultTime, itemType, size);
    DbNodeId dbNodeId = 0;
    bool constraintError = false;
    _syncpal->syncDb()->insertNode(dbNode, dbNodeId, constraintError);
    return dbNodeId;
}

void TestInitialSituationGenerator::insertInAllUpdateTrees(const NodeType itemType, const NodeId &id, const NodeId &parentId,
                                                           const DbNodeId dbNodeId) const {
    for (const auto side: {ReplicaSide::Local, ReplicaSide::Remote}) {
        (void) insertInUpdateTree(side, itemType, id, parentId, dbNodeId);
    }
}

} // namespace KDC
