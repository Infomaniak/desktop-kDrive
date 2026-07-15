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

#include "setinitialsituation.h"

#include "syncpal/syncpal.h"

#include "test_utility/testhelpers.h"

#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "jobs/network/kDrive_API/createdirjob.h"
#include "jobs/network/kDrive_API/upload/uploadjob.h"

#include <Poco/JSON/Array.h>
#include <Poco/JSON/Parser.h>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace KDC {

//
// ─────────────────────────────────────────────────
// Situation
// ─────────────────────────────────────────────────
//

Situation::Situation(const StringType &jsonDescription) :
    _jsonDescription(jsonDescription) {
    try {
        Poco::JSON::Parser parser;
        (void) parser.parse(SyncName2Str(jsonDescription)).extract<Poco::JSON::Object::Ptr>();
    } catch (Poco::Exception &) {
        throw SituationGeneratorException("Invalid Situation JSON");
    }
}

Situation Situation::fromFile(const std::filesystem::path &filePath) {
    const std::ifstream file(filePath, std::ios::binary);
    if (!file) throw SituationGeneratorException("Situation::fromFile: unable to open file: " + filePath.string());

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return Situation(Str2SyncName(buffer.str()));
}

const Situation::StringType &Situation::json() const noexcept {
    return _jsonDescription;
}

void Situation::log() const {
    LOGW_INFO(Log::instance()->getLogger(), SyncName2WStr(_jsonDescription));
}

//
// ─────────────────────────────────────────────────
// SetInitialSituation
// ─────────────────────────────────────────────────
//

SetInitialSituation::SetInitialSituation(const std::shared_ptr<SyncPal> syncPal) :
    _syncPal(syncPal) {}

void SetInitialSituation::setSyncpal(const std::shared_ptr<SyncPal> syncPal) {
    _syncPal = syncPal;
}

void SetInitialSituation::setRemoteDrive(const DriveDbId driveDbId, const NodeId &parentRemoteNodeId) {
    _remoteDriveDbId = driveDbId;
    _remoteRootId = parentRemoteNodeId;
    _remoteNodeIds[{}] = _remoteRootId;
}

bool SetInitialSituation::run(const std::string &jsonDescription) {
    if (!_syncPal) return false;

    try {
        // If remote is needed (optional depending on test)
        if (_remoteRootId.empty()) {
            setRemoteDrive(_syncPal->driveDbId(), *_syncPal->syncDb()->rootNode().nodeIdRemote());
        }

        const Situation situation{Str2SyncName(jsonDescription)};
        generateInitialSituation(situation);

        return true;
    } catch (const SituationGeneratorException &) {
        return false;
    }
}

void SetInitialSituation::generateInitialSituation(const Situation &situation) {
    if (!_syncPal) throw SituationGeneratorException("Invalid parameters!");

    Poco::JSON::Object::Ptr obj;
    try {
        Poco::JSON::Parser parser;
        obj = parser.parse(SyncName2Str(situation.json())).extract<Poco::JSON::Object::Ptr>();
    } catch (Poco::Exception &) {
        throw SituationGeneratorException("Invalid JSON input");
    }

    if (obj->isArray("content")) {
        addItem(obj->getArray("content"), {});
    } else {
        addItem(obj);
    }
}

void SetInitialSituation::addItem(Poco::JSON::Object::Ptr obj, const NodeId &parentId /*= {}*/) {
    std::vector<std::string> keys;
    obj->getNames(keys);

    for (const auto &key: keys) {
        const NodeType type = !obj->isObject(key) ? NodeType::File : NodeType::Directory;
        ItemDesc desc;
        desc.type = type;
        desc.id = parentId.empty() ? key : parentId + "/" + key;
        desc.name = Str2SyncName(CommonUtility::toUpper(key));
        desc.createdAt = testhelpers::defaultTime;
        desc.lastModifiedAt = testhelpers::defaultTime;
        desc.size = type == NodeType::File ? testhelpers::defaultFileSize : testhelpers::defaultDirSize;
        addItem(desc, parentId);

        if (obj->isObject(key)) {
            const auto &childObj = obj->getObject(key);
            addItem(childObj, desc.id);
        }

        if (desc.type == NodeType::Directory) setLocalItemDates(desc);
    }
}

void SetInitialSituation::addItem(Poco::JSON::Array::Ptr arr, const NodeId &parentId) {
    for (size_t i = 0; i < arr->size(); ++i) {
        const auto &itemObj = arr->getObject(static_cast<uint64_t>(i));
        if (!itemObj) throw SituationGeneratorException("Extended format: each 'content' element must be an object");

        const std::string typeStr = itemObj->optValue<std::string>("type", "File");
        const NodeType type = (typeStr == "Directory") ? NodeType::Directory : NodeType::File;
        const std::string nameStr = itemObj->optValue<std::string>("name", "");
        if (nameStr.empty()) throw SituationGeneratorException("Extended format: missing 'name' field");

        const std::string lowerName = CommonUtility::toLower(nameStr);

        ItemDesc desc;
        desc.type = type;
        desc.id = parentId.empty() ? lowerName : parentId + "/" + lowerName;
        desc.name = Str2SyncName(nameStr);
        desc.createdAt = itemObj->optValue<SyncTime>("createdAt", testhelpers::defaultTime);
        desc.lastModifiedAt = itemObj->optValue<SyncTime>("lastModifiedAt", testhelpers::defaultTime);
        desc.size = itemObj->optValue<int64_t>(
                "size", type == NodeType::File ? testhelpers::defaultFileSize : testhelpers::defaultDirSize);
        addItem(desc, parentId);

        if (type == NodeType::Directory && itemObj->isArray("content")) {
            addItem(itemObj->getArray("content"), desc.id);
        }

        if (desc.type == NodeType::Directory) setLocalItemDates(desc);
    }
}

void SetInitialSituation::addItem(const ItemDesc &desc, const NodeId &parentId) {
    insertLocalItem(desc, parentId);
    insertRemoteItem(desc, parentId);
}

void SetInitialSituation::insertLocalItem(const ItemDesc &desc, const NodeId &parentId) {
    const SyncPath namePath(desc.name);
    if (namePath.is_absolute() || namePath.filename() != namePath || namePath.empty()) {
        throw SituationGeneratorException("Invalid item name: '" + SyncName2Str(desc.name) + "'");
    }

    const SyncPath parentRelPath = parentId.empty() ? SyncPath{} : _localItemPaths.at(parentId);
    const SyncPath relPath = parentRelPath / namePath;
    _localItemPaths[desc.id] = relPath;
    const SyncPath localRoot = _syncPal->localPath().lexically_normal();
    const SyncPath fullPath = (localRoot / relPath).lexically_normal();

    // Ensure the resulting path stays within the local sync root (guards against traversal via "..").
    const auto [rootEnd, fullBegin] = std::mismatch(localRoot.begin(), localRoot.end(), fullPath.begin(), fullPath.end());
    if (rootEnd != localRoot.end()) {
        throw SituationGeneratorException("Item path escapes the local sync root: '" + fullPath.string() + "'");
    }

    IoError ioError = IoError::Success;
    if (desc.type == NodeType::Directory) {
        (void) IoHelper::createDirectory(fullPath, true, ioError);
    } else {
        (void) IoHelper::createDirectory(fullPath.parent_path(), true, ioError);
        testhelpers::generateTestFile(fullPath);
        if (desc.size > 0) testhelpers::setTestFileSize(fullPath, static_cast<uint64_t>(desc.size));
        setLocalItemDates(desc);
    }
}

void SetInitialSituation::setLocalItemDates(const ItemDesc &desc) const {
    const SyncPath fullPath = _syncPal->localPath() / _localItemPaths.at(desc.id);
    if (const IoError ioError = IoHelper::setFileDates(fullPath, desc.createdAt, desc.lastModifiedAt, false);
        ioError != IoError::Success) {
        throw SituationGeneratorException("Unable to set item dates for '" + fullPath.string() + "'");
    }
}

void SetInitialSituation::insertRemoteItem(const ItemDesc &desc, const NodeId &parentId) {
    if (!_remoteDriveDbId.has_value() || _remoteRootId.empty()) return;
    const NodeId parentRemoteId = parentId.empty() ? _remoteRootId : _remoteNodeIds.at(parentId);
    if (desc.type == NodeType::Directory) {
        CreateDirJob job(nullptr, *_remoteDriveDbId, parentRemoteId, desc.name);
        (void) job.runSynchronously();
        _remoteNodeIds[desc.id] = job.nodeId();
    } else {
        const SyncPath localFilePath = _syncPal->localPath() / _localItemPaths.at(desc.id);
        UploadJob job(nullptr, *_remoteDriveDbId, localFilePath, desc.name, parentRemoteId, desc.createdAt, desc.lastModifiedAt);
        (void) job.runSynchronously();
        _remoteNodeIds[desc.id] = job.nodeId();
    }
}

} // namespace KDC
