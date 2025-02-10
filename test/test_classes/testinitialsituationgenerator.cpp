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

static constexpr std::string localIdSuffix = "l_";
static constexpr std::string remoteIdSuffix = "r_";

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

std::shared_ptr<Node> TestInitialSituationGenerator::getNode(const ReplicaSide side, const NodeId &rawId) const {
    return _syncpal->updateTree(side)->getNodeById(generateId(side, rawId));
}

bool TestInitialSituationGenerator::getDbNode(const NodeId &rawId, DbNode &dbNode) const {
    bool found = false;
    if (!_syncpal->syncDb()->node(ReplicaSide::Local, generateId(ReplicaSide::Local, rawId), dbNode, found)) {
        return false;
    }
    return found;
}

std::shared_ptr<Node> TestInitialSituationGenerator::insertInUpdateTree(const ReplicaSide side, const NodeType itemType,
                                                                        const NodeId &rawId,
                                                                        const NodeId &parentRawId /*= ""*/) const {
    const auto parentNode = parentRawId.empty() ? _syncpal->updateTree(side)->rootNode()
                                                : _syncpal->updateTree(side)->getNodeById(generateId(side, parentRawId));
    const auto size = itemType == NodeType::File ? testhelpers::defaultFileSize : testhelpers::defaultDirSize;
    const auto node =
            std::make_shared<Node>(side, Utility::toUpper(rawId), itemType, OperationType::None, generateId(side, rawId),
                                   testhelpers::defaultTime, testhelpers::defaultTime, size, parentNode);
    _syncpal->updateTree(side)->insertNode(node);
    (void) parentNode->insertChildren(node);
    return node;
}

void TestInitialSituationGenerator::moveNode(const ReplicaSide side, const NodeId &rawId, const NodeId &newParentRawId) const {
    const auto newParentNode = newParentRawId.empty() ? _syncpal->updateTree(side)->rootNode()
                                                      : _syncpal->updateTree(side)->getNodeById(generateId(side, newParentRawId));
    const auto node = _syncpal->updateTree(side)->getNodeById(generateId(side, rawId));

    node->setMoveOrigin(node->getPath());
    (void) node->parentNode()->deleteChildren(node);
    (void) node->setParentNode(newParentNode);
    (void) newParentNode->insertChildren(node);
}

void TestInitialSituationGenerator::removeFromUpdateTree(const ReplicaSide side, const NodeId &rawId) const {
    (void) _syncpal->updateTree(side)->deleteNode(rawId);
}

NodeId TestInitialSituationGenerator::generateId(const ReplicaSide side, const NodeId &rawId) const {
    if (rawId.starts_with(localIdSuffix) || rawId.starts_with(remoteIdSuffix)) return rawId;
    return side == ReplicaSide::Local ? localIdSuffix + rawId : remoteIdSuffix + rawId;
}

void TestInitialSituationGenerator::addItem(Poco::JSON::Object::Ptr obj, const NodeId &parentRawId /*= {}*/) {
    std::vector<std::string> keys;
    obj->getNames(keys);

    for (const auto &key: keys) {
        const auto isFile = !obj->isObject(key) && obj->getValue<bool>(key);
        addItem(isFile ? NodeType::File : NodeType::Directory, key, parentRawId);

        if (obj->isObject(key)) {
            const auto &childObj = obj->getObject(key);
            addItem(childObj, key);
        }
    }
}

void TestInitialSituationGenerator::addItem(const NodeType itemType, const NodeId &rawId, const NodeId &parentRawId) const {
    insertInDb(itemType, rawId, parentRawId);
    insertInAllUpdateTrees(itemType, rawId, parentRawId);
}

void TestInitialSituationGenerator::insertInDb(const NodeType itemType, const NodeId &rawId, const NodeId &parentRawId) const {
    DbNode parentNode;
    if (parentRawId.empty()) {
        parentNode = _syncpal->syncDb()->rootNode();
    } else {
        bool found = false;
        if (!_syncpal->syncDb()->node(ReplicaSide::Local, generateId(ReplicaSide::Local, parentRawId), parentNode, found)) {
            throw std::runtime_error("Failed to find parent node");
        }
        if (!found) {
            throw std::runtime_error("Failed to find parent node");
        }
    }

    const auto size = itemType == NodeType::File ? testhelpers::defaultFileSize : testhelpers::defaultDirSize;
    const DbNode dbNode(parentNode.nodeId(), Utility::toUpper(rawId), Utility::toUpper(rawId),
                        generateId(ReplicaSide::Local, rawId), generateId(ReplicaSide::Remote, rawId), testhelpers::defaultTime,
                        testhelpers::defaultTime, testhelpers::defaultTime, itemType, size, std::nullopt);
    _syncpal->syncDb()->insertNode(dbNode);
}

void TestInitialSituationGenerator::insertInAllUpdateTrees(const NodeType itemType, const NodeId &rawId,
                                                           const NodeId &parentRawId) const {
    for (const auto side: {ReplicaSide::Local, ReplicaSide::Remote}) {
        (void) insertInUpdateTree(side, itemType, rawId, parentRawId);
    }
}

} // namespace KDC
