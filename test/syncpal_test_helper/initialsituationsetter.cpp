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

#include "initialsituationsetter.h"

#include "syncpal/syncpal.h"

#include "test_utility/testhelpers.h"

#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "jobs/network/kDrive_API/createdirjob.h"
#include "jobs/network/kDrive_API/upload/uploadjob.h"

#include <Poco/JSON/Array.h>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>

#include <chrono>
#include <fstream>
#include <sstream>

namespace KDC {

//
// ─────────────────────────────────────────────────
// Situation
// ─────────────────────────────────────────────────
//

Situation::Situation(const SyncName &jsonDescription) {
    try {
        Poco::JSON::Parser parser;
        _jsonObject = parser.parse(SyncName2Str(jsonDescription)).extract<Poco::JSON::Object::Ptr>();
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

const Poco::JSON::Object::Ptr &Situation::jsonObject() const noexcept {
    return _jsonObject;
}

bool Situation::operator==(const Situation &other) const noexcept {
    std::ostringstream oss;
    Poco::JSON::Stringifier::stringify(_jsonObject, oss, /*indent=*/0);
    std::ostringstream otherOss;
    Poco::JSON::Stringifier::stringify(other._jsonObject, otherOss, /*indent=*/0);
    return oss.str() == otherOss.str();
}

void Situation::log() const {
    std::ostringstream oss;
    Poco::JSON::Stringifier::stringify(_jsonObject, oss, 2);
    LOGW_INFO(Log::instance()->getLogger(), CommonUtility::s2ws(oss.str()));
}

//
// ─────────────────────────────────────────────────
// InitialSituationSetter
// ─────────────────────────────────────────────────
//

InitialSituationSetter::InitialSituationSetter(const std::shared_ptr<SyncPal> syncPal) {
    setSyncpal(syncPal);
}

void InitialSituationSetter::setSyncpal(const std::shared_ptr<SyncPal> syncPal) {
    _syncPal = syncPal;

    _remoteNodeIds.clear();
    if (_syncPal) {
        _remoteNodeIds[{}] = *_syncPal->syncDb()->rootNode().nodeIdRemote();
    }
}

bool InitialSituationSetter::run(const std::string &localJsonDescription, const std::string &remoteJsonDescription) {
    if (!_syncPal) return false;

    try {
        const Situation localSituation{Str2SyncName(localJsonDescription)};
        const Situation remoteSituation{Str2SyncName(remoteJsonDescription)};
        generateInitialSituation(localSituation, remoteSituation);

        return true;
    } catch (const SituationGeneratorException &e) {
        LOG_WARN(Log::instance()->getLogger(), "InitialSituationSetter::run: " << e.what());
        return false;
    }
}

void InitialSituationSetter::generateInitialSituation(const Situation &localSituation, const Situation &remoteSituation) {
    if (!_syncPal) throw SituationGeneratorException("Invalid parameters!");

    generateSituation(localSituation, ReplicaSide::Local);
    generateSituation(remoteSituation, ReplicaSide::Remote);
}

void InitialSituationSetter::generateSituation(const Situation &situation, const ReplicaSide side) {
    const auto &obj = situation.jsonObject();
    if (obj->isArray("content")) {
        addItem(side, obj->getArray("content"), {});
    } else {
        // No "content" array: legacy format, where the object's own keys are the items.
        addItem(side, obj);
    }
}

void InitialSituationSetter::addItem(const ReplicaSide side, Poco::JSON::Object::Ptr obj, const NodeId &parentId /*= {}*/) {
    std::vector<std::string> keys;
    obj->getNames(keys);

    for (const auto &key: keys) {
        const NodeType type = obj->isObject(key) ? NodeType::Directory : NodeType::File;
        ItemDesc desc;
        desc.type = type;
        desc.id = parentId.empty() ? key : parentId + "/" + key;
        desc.name = Str2SyncName(CommonUtility::toUpper(key));
        desc.size = type == NodeType::File ? testhelpers::defaultFileSize : testhelpers::defaultDirSize;
        addItem(side, desc, parentId);

        if (obj->isObject(key)) {
            const auto &childObj = obj->getObject(key);
            addItem(side, childObj, desc.id);
        }
    }
}

void InitialSituationSetter::addItem(const ReplicaSide side, Poco::JSON::Array::Ptr arr, const NodeId &parentId) {
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
        desc.size = itemObj->optValue<int64_t>(
                "size", type == NodeType::File ? testhelpers::defaultFileSize : testhelpers::defaultDirSize);
        addItem(side, desc, parentId);

        if (type == NodeType::Directory && itemObj->isArray("content")) {
            addItem(side, itemObj->getArray("content"), desc.id);
        }
    }
}

void InitialSituationSetter::addItem(const ReplicaSide side, const ItemDesc &desc, const NodeId &parentId) {
    if (side == ReplicaSide::Local) {
        insertLocalItem(desc, parentId);
    } else {
        insertRemoteItem(desc, parentId);
    }
}

void InitialSituationSetter::insertLocalItem(const ItemDesc &desc, const NodeId &parentId) {
    const SyncPath namePath(desc.name);
    if (namePath.is_absolute() || namePath.filename() != namePath || namePath.empty()) {
        throw SituationGeneratorException("Invalid item name: '" + SyncName2Str(desc.name) + "'");
    }

    SyncPath parentRelPath;
    if (!parentId.empty()) {
        try {
            parentRelPath = _localItemPaths.at(parentId);
        } catch (const std::out_of_range &) {
            throw SituationGeneratorException("Unknown parent item id: '" + parentId + "'");
        }
    }
    const SyncPath relPath = parentRelPath / namePath;
    _localItemPaths[desc.id] = relPath;
    const SyncPath localRoot = _syncPal->localPath().lexically_normal();
    const SyncPath fullPath = (localRoot / relPath).lexically_normal();

    // Ensure the resulting path stays within the local sync root (guards against traversal via "..").
    if (!CommonUtility::isSubDir(localRoot, fullPath)) {
        throw SituationGeneratorException("Item path escapes the local sync root: '" + fullPath.string() + "'");
    }

    IoError ioError = IoError::Success;
    if (desc.type == NodeType::Directory) {
        (void) IoHelper::createDirectory(fullPath, true, ioError);
    } else {
        testhelpers::generateTestFile(fullPath);
        if (desc.size > 0) testhelpers::setTestFileSize(fullPath, static_cast<uint64_t>(desc.size));
    }
}

SyncPath InitialSituationSetter::localFilePathForUpload(const ItemDesc &desc) {
    if (const auto it = _localItemPaths.find(desc.id); it != _localItemPaths.end()) {
        return _syncPal->localPath() / it->second;
    }

    // Remote-only item: no local counterpart was generated, create a scratch file to upload from.
    const LocalTemporaryDirectory &uploadScratchDir =
            _uploadScratchDir ? *_uploadScratchDir : _uploadScratchDir.emplace("InitialSituationSetterUpload");
    const SyncPath scratchPath = uploadScratchDir.path() / desc.name;
    testhelpers::generateTestFile(scratchPath);
    if (desc.size > 0) testhelpers::setTestFileSize(scratchPath, static_cast<uint64_t>(desc.size));
    return scratchPath;
}

NodeId InitialSituationSetter::remoteParentId(const NodeId &parentId) const {
    try {
        return _remoteNodeIds.at(parentId);
    } catch (const std::out_of_range &) {
        throw SituationGeneratorException("Unknown parent item id: '" + parentId + "'");
    }
}

void InitialSituationSetter::insertRemoteItem(const ItemDesc &desc, const NodeId &parentId) {
    if (_remoteNodeIds.at({}).empty()) return;

    try {
        const NodeId parentRemoteId = remoteParentId(parentId);
        if (desc.type == NodeType::Directory) {
            CreateDirJob job(nullptr, _syncPal->driveDbId(), parentRemoteId, desc.name);
            (void) job.runSynchronously();
            _remoteNodeIds[desc.id] = job.nodeId();
        } else {
            const SyncPath localFilePath = localFilePathForUpload(desc);
            // UploadJob requires creation/modification times structurally; no date semantics are relevant here, so
            // the current time is used.
            const auto now = static_cast<SyncTime>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
            UploadJob job(nullptr, _syncPal->driveDbId(), localFilePath, desc.name, parentRemoteId, now, now);
            (void) job.runSynchronously();
            _remoteNodeIds[desc.id] = job.nodeId();
        }
    } catch (const SituationGeneratorException &e) {
        LOG_WARN(Log::instance()->getLogger(), "InitialSituationSetter::insertRemoteItem: " << e.what());
        throw;
    } catch (const std::exception &e) {
        throw SituationGeneratorException(std::string("Failed to insert remote item: ") + e.what());
    }
}

} // namespace KDC
