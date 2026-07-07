/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "situationgenerator.h"
#include "update_detection/file_system_observer/filesystemobserverworker.h"
#include "db/dbnode.h"
#include "syncpal/syncpal.h"

#include "test_utility/testhelpers.h"

#include "libcommonserver/io/iohelper.h"
#include "jobs/network/kDrive_API/createdirjob.h"
#include "jobs/network/kDrive_API/upload/uploadjob.h"

#include <filesystem>

#include <Poco/JSON/Array.h>
#include <Poco/JSON/Parser.h>

namespace KDC {

static const std::string localIdSuffix = "l_";
static const std::string remoteIdSuffix = "r_";

class SituationGeneratorException final : public std::runtime_error {
    public:
        explicit SituationGeneratorException(const std::string &what) :
            std::runtime_error(what) {}
};

SituationGenerator::SituationGenerator() {
    const auto syncDbPath = _temporaryDirectory.path() / ("dummySyncDb_" + CommonUtility::generateRandomStringAlphaNum());
    _syncDb = std::make_shared<SyncDb>(syncDbPath.string());
    (void) _syncDb->init(KDRIVE_VERSION_STRING);
    _syncDb->setAutoDelete(true);

    _localUpdateTree = std::make_shared<UpdateTree>(ReplicaSide::Local, SyncDb::driveRootNode());
    _remoteUpdateTree = std::make_shared<UpdateTree>(ReplicaSide::Remote, SyncDb::driveRootNode());
}

SituationGenerator::SituationGenerator(const std::shared_ptr<SyncPal> syncpal) :
    _syncDb(syncpal->syncDb()),
    _localLiveSnapshot(syncpal->_localFSObserverWorker->_liveSnapshot),
    _remoteLiveSnapshot(syncpal->_remoteFSObserverWorker->_liveSnapshot),
    _localUpdateTree(syncpal->updateTree(ReplicaSide::Local)),
    _remoteUpdateTree(syncpal->updateTree(ReplicaSide::Remote)) {}

void SituationGenerator::setSyncpal(const std::shared_ptr<SyncPal> syncpal) {
    _syncDb = syncpal->syncDb();
    _localLiveSnapshot = syncpal->_localFSObserverWorker->_liveSnapshot;
    _remoteLiveSnapshot = syncpal->_remoteFSObserverWorker->_liveSnapshot;
    _localUpdateTree = syncpal->updateTree(ReplicaSide::Local);
    _remoteUpdateTree = syncpal->updateTree(ReplicaSide::Remote);
}

void SituationGenerator::setRemoteDrive(const DriveDbId driveDbId, const NodeId &parentRemoteNodeId) {
    _remoteDriveDbId = driveDbId;
    _remoteItemDir = std::make_unique<RemoteTemporaryDirectory>(driveDbId, parentRemoteNodeId, "TestSituationGenerator");
    _remoteNodeIds[{}] = _remoteItemDir->id();
}

void SituationGenerator::generateInitialSituation(const std::string &jsonInputStr) {
    if (!_syncDb || !_localUpdateTree || !_remoteUpdateTree) throw std::runtime_error("Invalid parameters!");

    Poco::JSON::Object::Ptr obj;
    try {
        Poco::JSON::Parser parser;
        obj = parser.parse(jsonInputStr).extract<Poco::JSON::Object::Ptr>();
    } catch (Poco::Exception &) {
        throw SituationGeneratorException("Invalid JSON input");
    }

    if (obj->isArray("content")) {
        addItem(obj->getArray("content"), {});
    } else {
        addItem(obj);
    }

    _localUpdateTree->drawUpdateTree();
    (void) _syncDb->cache().reloadIfNeeded();
}

std::shared_ptr<Node> SituationGenerator::getNode(const ReplicaSide side, const NodeId &id) const {
    return updateTree(side)->getNodeById(generateId(side, id));
}

bool SituationGenerator::getDbNode(const NodeId &id, DbNode &dbNode) const {
    bool found = false;
    if (!_syncDb->node(ReplicaSide::Local, generateId(ReplicaSide::Local, id), dbNode, found)) {
        return false;
    }
    return found;
}

std::shared_ptr<Node> SituationGenerator::createNode(const ReplicaSide side, const NodeType itemType, const NodeId &id,
                                                         const NodeId &parentId, const bool setChangeEvent) const {
    const auto node = insertInUpdateTree(side, itemType, id, parentId, std::nullopt);
    if (setChangeEvent) node->setChangeEvents(OperationType::Create);
    return node;
}

std::shared_ptr<Node> SituationGenerator::moveNode(const ReplicaSide side, const NodeId &id, const NodeId &newParentId,
                                                       const SyncName &newName /*= {}*/) const {
    const auto newParentNode =
            newParentId.empty() ? updateTree(side)->rootNode() : updateTree(side)->getNodeById(generateId(side, newParentId));
    const auto node = updateTree(side)->getNodeById(generateId(side, id));

    node->setMoveOriginInfos({node->getPath(), node->parentNode()->id().value()});
    (void) node->parentNode()->deleteChild(node);
    (void) node->setParentNode(newParentNode);
    (void) newParentNode->insertChild(node);
    if (!newName.empty()) node->setName(newName);
    node->insertChangeEvent(OperationType::Move);
    return node;
}

std::shared_ptr<Node> SituationGenerator::renameNode(const ReplicaSide side, const NodeId &id,
                                                         const SyncName &newName) const {
    const auto node = updateTree(side)->getNodeById(generateId(side, id));
    node->setName(newName);
    node->setMoveOriginInfos({node->getPath(), node->parentNode()->id().value()});
    node->insertChangeEvent(OperationType::Move);
    return node;
}

[[maybe_unused]] std::shared_ptr<Node> SituationGenerator::editNode(const ReplicaSide side, const NodeId &id,
                                                                        const SyncTime timeInput /*= 0*/) const {
    static uint64_t editCounter = 0; // Make sure that 2 consecutive edit operations do not generate the same operation.
    const auto node = updateTree(side)->getNodeById(generateId(side, id));
    SyncTime modificationTime = 0;
    if (timeInput) {
        modificationTime = timeInput;
    } else {
        modificationTime += node->modificationTime().value() + static_cast<SyncTime>(++editCounter);
    }
    node->setModificationTime(modificationTime);
    node->insertChangeEvent(OperationType::Edit);
    return node;
}

std::shared_ptr<Node> SituationGenerator::deleteNode(const ReplicaSide side, const NodeId &id) const {
    const auto node = getNode(side, id);
    node->setChangeEvents(OperationType::Delete);
    for (const auto &[_, child]: node->children()) {
        child->setChangeEvents(OperationType::Delete);
    }
    return node;
}

NodeId SituationGenerator::generateId(const ReplicaSide side, const NodeId &id) const {
    NodeId rawId;
    if (id.starts_with(localIdSuffix)) {
        rawId = id.substr(localIdSuffix.size());
    } else if (id.starts_with(remoteIdSuffix)) {
        rawId = id.substr(remoteIdSuffix.size());
    } else {
        rawId = id;
    }
    return (side == ReplicaSide::Local ? localIdSuffix : remoteIdSuffix) + rawId;
}

void SituationGenerator::addItem(Poco::JSON::Object::Ptr obj, const NodeId &parentId /*= {}*/) {
    std::vector<std::string> keys;
    obj->getNames(keys);

    for (const auto &key: keys) {
        const NodeType type = !obj->isObject(key) ? NodeType::File : NodeType::Directory;
        ItemDesc desc;
        desc.type = type;
        desc.id = key;
        desc.name = Str2SyncName(CommonUtility::toUpper(key));
        desc.createdAt = testhelpers::defaultTime;
        desc.lastModifiedAt = testhelpers::defaultTime;
        desc.size = type == NodeType::File ? testhelpers::defaultFileSize : testhelpers::defaultDirSize;
        addItem(desc, parentId);

        if (obj->isObject(key)) {
            const auto &childObj = obj->getObject(key);
            addItem(childObj, key);
        }
    }
}

void SituationGenerator::addItem(Poco::JSON::Array::Ptr arr, const NodeId &parentId) {
    for (size_t i = 0; i < arr->size(); ++i) {
        const auto &itemObj = arr->getObject(static_cast<unsigned int>(i));
        if (!itemObj) throw SituationGeneratorException("Extended format: each 'content' element must be an object");

        const std::string typeStr = itemObj->optValue<std::string>("type", "File");
        const NodeType type = (typeStr == "Directory") ? NodeType::Directory : NodeType::File;
        const std::string nameStr = itemObj->optValue<std::string>("name", "");
        if (nameStr.empty()) throw SituationGeneratorException("Extended format: missing 'name' field");

        ItemDesc desc;
        desc.type = type;
        desc.id = CommonUtility::toLower(nameStr);
        desc.name = Str2SyncName(nameStr);
        desc.createdAt = itemObj->optValue<SyncTime>("createdAt", testhelpers::defaultTime);
        desc.lastModifiedAt = itemObj->optValue<SyncTime>("lastModifiedAt", testhelpers::defaultTime);
        desc.size = itemObj->optValue<int64_t>(
                "size", type == NodeType::File ? testhelpers::defaultFileSize : testhelpers::defaultDirSize);
        addItem(desc, parentId);

        if (type == NodeType::Directory && itemObj->isArray("content")) {
            addItem(itemObj->getArray("content"), desc.id);
        }
    }
}

void SituationGenerator::addItem(const ItemDesc &desc, const NodeId &parentId) const {
    insertLocalItem(desc, parentId);
    insertRemoteItem(desc, parentId);
    insertInAllSnapshot(desc, parentId);
    const DbNodeId dbNodeId = insertInDb(desc, parentId);
    insertInAllUpdateTrees(desc, parentId, dbNodeId);
}

void SituationGenerator::addItem(const NodeType itemType, const NodeId &id, const NodeId &parentId) const {
    ItemDesc desc;
    desc.type = itemType;
    desc.id = id;
    desc.name = Str2SyncName(CommonUtility::toUpper(id));
    desc.createdAt = testhelpers::defaultTime;
    desc.lastModifiedAt = testhelpers::defaultTime;
    desc.size = itemType == NodeType::File ? testhelpers::defaultFileSize : testhelpers::defaultDirSize;
    addItem(desc, parentId);
}

size_t SituationGenerator::size() const {
    return _localUpdateTree->nodes().size();
}

void SituationGenerator::insertLocalItem(const ItemDesc &desc, const NodeId &parentId) const {
    const SyncPath parentRelPath = parentId.empty() ? SyncPath{} : _localItemPaths.at(parentId);
    const SyncPath relPath = parentRelPath / desc.name;
    _localItemPaths[desc.id] = relPath;
    const SyncPath fullPath = _localItemDir.path() / relPath;
    IoError ioError = IoError::Success;
    if (desc.type == NodeType::Directory) {
        (void) IoHelper::createDirectory(fullPath, true, ioError);
    } else {
        (void) IoHelper::createDirectory(fullPath.parent_path(), true, ioError);
        testhelpers::generateTestFile(fullPath);
        if (desc.size > 0) testhelpers::setTestFileSize(fullPath, static_cast<uint64_t>(desc.size));
    }
    NodeId nodeId;
    if (IoHelper::getNodeId(fullPath, nodeId)) _localNodeIds[desc.id] = nodeId;
}

void SituationGenerator::insertRemoteItem(const ItemDesc &desc, const NodeId &parentId) const {
    if (!_remoteDriveDbId || !_remoteItemDir) return;
    const NodeId parentRemoteId = parentId.empty() ? _remoteItemDir->id() : _remoteNodeIds.at(parentId);
    if (desc.type == NodeType::Directory) {
        CreateDirJob job(nullptr, *_remoteDriveDbId, parentRemoteId, desc.name);
        (void) job.runSynchronously();
        _remoteNodeIds[desc.id] = job.nodeId();
    } else {
        const SyncPath localFilePath = _localItemDir.path() / _localItemPaths.at(desc.id);
        UploadJob job(nullptr, *_remoteDriveDbId, localFilePath, desc.name, parentRemoteId, desc.createdAt, desc.lastModifiedAt);
        (void) job.runSynchronously();
        _remoteNodeIds[desc.id] = job.nodeId();
    }
}

void SituationGenerator::insertInAllSnapshot(const ItemDesc &desc, const NodeId &parentId) const {
    if (desc.id.empty()) return;
    for (const auto side: {ReplicaSide::Local, ReplicaSide::Remote}) {
        if (!(side == ReplicaSide::Local ? _localLiveSnapshot : _remoteLiveSnapshot).has_value()) continue;

        const auto &nodeIdMap = (side == ReplicaSide::Local) ? _localNodeIds : _remoteNodeIds;
        const auto nodeIdIt = nodeIdMap.find(desc.id);
        const NodeId snapshotId = (nodeIdIt != nodeIdMap.end()) ? nodeIdIt->second : generateId(side, desc.id);

        NodeId snapshotParentId;
        if (parentId.empty()) {
            snapshotParentId = (side == ReplicaSide::Local)
                                       ? _localItemDir.id()
                                       : (_remoteItemDir ? _remoteItemDir->id()
                                                         : *_syncDb->rootNode().nodeIdRemote());
        } else {
            const auto parentIt = nodeIdMap.find(parentId);
            snapshotParentId =
                    (parentIt != nodeIdMap.end()) ? parentIt->second : generateId(side, parentId);
        }

        const SnapshotItem item(snapshotId, snapshotParentId, desc.name, desc.createdAt, desc.lastModifiedAt, desc.type,
                                desc.size, false, true, true);
        (void) liveSnapshot(side).updateItem(item);
    }
}

DbNodeId SituationGenerator::insertInDb(const ItemDesc &desc, const NodeId &parentId) const {
    DbNode parentNode;
    if (parentId.empty()) {
        parentNode = _syncDb->rootNode();
    } else {
        bool found = false;
        if (!_syncDb->node(ReplicaSide::Local, generateId(ReplicaSide::Local, parentId), parentNode, found)) {
            throw SituationGeneratorException("Failed to find parent node");
        }
        if (!found) {
            throw SituationGeneratorException("Failed to find parent node");
        }
    }

    const DbNode dbNode(parentNode.nodeId(), desc.name, desc.name, generateId(ReplicaSide::Local, desc.id),
                        generateId(ReplicaSide::Remote, desc.id), desc.createdAt, desc.lastModifiedAt, desc.lastModifiedAt,
                        desc.type, desc.size);
    DbNodeId dbNodeId = 0;
    bool constraintError = false;
    (void) _syncDb->insertNode(dbNode, dbNodeId, constraintError);
    return dbNodeId;
}

std::shared_ptr<Node> SituationGenerator::insertInUpdateTree(
        const ReplicaSide side, const NodeType itemType, const NodeId &id, const NodeId &parentId /*= ""*/,
        const std::optional<DbNodeId> dbNodeId /*= std::nullopt*/) const {
    const auto parentNode =
            parentId.empty() ? updateTree(side)->rootNode() : updateTree(side)->getNodeById(generateId(side, parentId));
    const auto size = itemType == NodeType::File ? testhelpers::defaultFileSize : testhelpers::defaultDirSize;
    const auto node =
            std::make_shared<Node>(dbNodeId, side, Str2SyncName(CommonUtility::toUpper(id)), itemType, OperationType::None,
                                   generateId(side, id), testhelpers::defaultTime, testhelpers::defaultTime, size, parentNode);
    updateTree(side)->insertNode(node);
    (void) parentNode->insertChild(node);
    return node;
}

std::shared_ptr<Node> SituationGenerator::insertInUpdateTree(const ReplicaSide side, const ItemDesc &desc,
                                                                 const NodeId &parentId,
                                                                 const std::optional<DbNodeId> dbNodeId) const {
    const auto parentNode =
            parentId.empty() ? updateTree(side)->rootNode() : updateTree(side)->getNodeById(generateId(side, parentId));
    const auto node = std::make_shared<Node>(dbNodeId, side, desc.name, desc.type, OperationType::None, generateId(side, desc.id),
                                             desc.createdAt, desc.lastModifiedAt, desc.size, parentNode);
    updateTree(side)->insertNode(node);
    (void) parentNode->insertChild(node);
    return node;
}

void SituationGenerator::insertInAllUpdateTrees(const ItemDesc &desc, const NodeId &parentId, const DbNodeId dbNodeId) const {
    for (const auto side: {ReplicaSide::Local, ReplicaSide::Remote}) {
        (void) insertInUpdateTree(side, desc, parentId, dbNodeId);
    }
}

} // namespace KDC
