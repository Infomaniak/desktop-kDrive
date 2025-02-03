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

NodeId TestInitialSituationGenerator::generateId(const ReplicaSide side, const NodeId &rawId) const {
    return side == ReplicaSide::Local ? "l_" + rawId : "r_" + rawId;
}

void TestInitialSituationGenerator::addItem(Poco::JSON::Object::Ptr obj, const NodeId &parentId /*= {}*/) {
    std::vector<std::string> keys;
    obj->getNames(keys);

    for (const auto &key: keys) {
        const auto isFile = !obj->isObject(key) && obj->getValue<bool>(key);
        addItem(isFile ? NodeType::File : NodeType::Directory, key, parentId);

        if (obj->isObject(key)) {
            const auto &childObj = obj->getObject(key);
            addItem(childObj, key);
        }
    }
}

void TestInitialSituationGenerator::addItem(const NodeType itemType, const NodeId &id, const NodeId &parentId) const {
    insertInDb(itemType, id, parentId);
    insertInUpdateTrees(itemType, id, parentId);
}

void TestInitialSituationGenerator::insertInDb(const NodeType itemType, const NodeId &id, const NodeId &parentId) const {
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
    const DbNode dbNode(parentNode.nodeId(), Utility::toUpper(id), Utility::toUpper(id), generateId(ReplicaSide::Local, id),
                        generateId(ReplicaSide::Remote, id), testhelpers::defaultTime, testhelpers::defaultTime,
                        testhelpers::defaultTime, itemType, size, std::nullopt);
    _syncpal->syncDb()->insertNode(dbNode);
}

void TestInitialSituationGenerator::insertInUpdateTrees(const NodeType itemType, const NodeId &id, const NodeId &parentId) const {
    for (const auto side: {ReplicaSide::Local, ReplicaSide::Remote}) {
        insertInUpdateTrees(side, itemType, id, parentId);
    }
}

void TestInitialSituationGenerator::insertInUpdateTrees(ReplicaSide side, NodeType itemType, const NodeId &id,
                                                        const NodeId &parentId) const {
    const auto parentNode = parentId.empty() ? _syncpal->updateTree(side)->rootNode()
                                             : _syncpal->updateTree(side)->getNodeById(generateId(side, parentId));
    const auto size = itemType == NodeType::File ? testhelpers::defaultFileSize : testhelpers::defaultDirSize;
    const auto node = std::make_shared<Node>(side, Utility::toUpper(id), itemType, OperationType::None, generateId(side, id),
                                             testhelpers::defaultTime, testhelpers::defaultTime, size, parentNode);
    _syncpal->updateTree(side)->insertNode(node);
    (void) parentNode->insertChildren(node);
}

} // namespace KDC
