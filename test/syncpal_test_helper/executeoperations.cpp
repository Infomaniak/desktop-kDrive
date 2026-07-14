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

#include "executeoperations.h"

#include "syncpal/syncpal.h"
#include "update_detection/file_system_observer/filesystemobserverworker.h"
#include "test_utility/testhelpers.h"
#include "test_utility/localtemporarydirectory.h"

#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"

#include "jobs/local/localcreatedirjob.h"
#include "jobs/local/synclocaldeletejob.h"
#include "jobs/local/localmovejob.h"
#include "jobs/local/localcopyjob.h"

#include "jobs/network/kDrive_API/createdirjob.h"
#include "jobs/network/kDrive_API/deletejob.h"
#include "jobs/network/kDrive_API/movejob.h"
#include "jobs/network/kDrive_API/renamejob.h"
#include "jobs/network/kDrive_API/copytodirectoryjob.h"
#include "jobs/network/kDrive_API/upload/uploadjob.h"

#include "db/syncdb.h"

#include <Poco/JSON/Array.h>
#include <Poco/JSON/Parser.h>

#include <fstream>
#include <sstream>

namespace KDC {

//
// ─────────────────────────────────────────────────
// Operations
// ─────────────────────────────────────────────────
//

Operations::Operations(const StringType &jsonDescription) :
    _jsonDescription(jsonDescription) {
    Poco::JSON::Object::Ptr obj;
    try {
        Poco::JSON::Parser parser;
        obj = parser.parse(SyncName2Str(jsonDescription)).extract<Poco::JSON::Object::Ptr>();
    } catch (Poco::Exception &) {
        throw OperationsParserException("Invalid Operations JSON");
    }

    if (!obj->has("operations") || !obj->isArray("operations")) {
        throw OperationsParserException("Operations must contain an 'operations' array");
    }
}

Operations Operations::fromFile(const std::filesystem::path &filePath) {
    const std::ifstream file(filePath, std::ios::binary);
    if (!file) throw OperationsParserException("Operations::fromFile: unable to open file: " + filePath.string());

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return Operations(Str2SyncName(buffer.str()));
}

const Operations::StringType &Operations::json() const noexcept {
    return _jsonDescription;
}

void Operations::log() const {
    LOGW_INFO(Log::instance()->getLogger(), SyncName2WStr(_jsonDescription));
}

//
// ─────────────────────────────────────────────────
// ExecuteOperations
// ─────────────────────────────────────────────────
//

ExecuteOperations::ExecuteOperations(const std::shared_ptr<SyncPal> syncPal) :
    _syncPal(syncPal) {}

bool ExecuteOperations::run(const ReplicaSide side, const std::string &jsonDescription) {
    if (!_syncPal) return false;

    try {
        const Operations operations{Str2SyncName(jsonDescription)};
        executeOperations(side, operations);

        return true;
    } catch (const OperationsParserException &) {
        return false;
    }
}

void ExecuteOperations::executeOperations(const ReplicaSide side, const Operations &operations) {
    Poco::JSON::Object::Ptr obj;
    try {
        Poco::JSON::Parser parser;
        obj = parser.parse(SyncName2Str(operations.json())).extract<Poco::JSON::Object::Ptr>();
    } catch (Poco::Exception &) {
        throw OperationsParserException("Invalid JSON input");
    }

    _batchRemoteIds.clear();

    const auto arr = obj->getArray("operations");
    for (size_t i = 0; i < arr->size(); ++i) {
        const auto &itemObj = arr->getObject(static_cast<uint64_t>(i));
        const OperationDesc desc = parseOperation(itemObj);
        applyOperation(side, desc);
    }

    if (side == ReplicaSide::Remote && _syncPal->_remoteFSObserverWorker) {
        // Remote changes are otherwise only picked up via long-poll/cursor, which can lag behind the
        // real API calls above. Force an immediate update so the caller's subsequent sync wait detects them.
        _syncPal->_remoteFSObserverWorker->forceUpdate();
    }
}

void ExecuteOperations::validateRelativePath(const SyncPath &path, const std::string &fieldName) {
    if (path.empty() || path.is_absolute()) {
        throw OperationsParserException("'" + fieldName + "' must be a non-empty relative path: '" + path.string() + "'");
    }
    for (const auto &part: path) {
        if (part == "..") {
            throw OperationsParserException("'" + fieldName + "' must not contain '..' components: '" + path.string() + "'");
        }
    }
}

ExecuteOperations::OperationDesc ExecuteOperations::parseOperation(const Poco::JSON::Object::Ptr &obj) {
    if (!obj) throw OperationsParserException("Each operation must be an object");

    const std::string typeStr = obj->optValue<std::string>("type", "");

    OperationDesc desc;
    if (typeStr == "Create") {
        desc.type = OperationType::Create;

        const std::string itemTypeStr = obj->optValue<std::string>("itemType", "File");
        desc.itemType = (itemTypeStr == "Directory") ? NodeType::Directory : NodeType::File;

        const std::string nameStr = obj->optValue<std::string>("name", "");
        if (nameStr.empty()) throw OperationsParserException("Create operation missing 'name'");
        desc.path = Str2Path(nameStr);
        validateRelativePath(desc.path, "name");

        desc.size = obj->optValue<int64_t>(
                "size", desc.itemType == NodeType::File ? testhelpers::defaultFileSize : testhelpers::defaultDirSize);
        desc.createdAt = obj->optValue<SyncTime>("createdAt", testhelpers::defaultTime);
        desc.lastModifiedAt = obj->optValue<SyncTime>("lastModifiedAt", testhelpers::defaultTime);
    } else if (typeStr == "Edit") {
        desc.type = OperationType::Edit;

        const std::string pathStr = obj->optValue<std::string>("path", "");
        if (pathStr.empty()) throw OperationsParserException("Edit operation missing 'path'");
        desc.path = Str2Path(pathStr);
        validateRelativePath(desc.path, "path");

        desc.size = obj->optValue<int64_t>("newSize", 0);
        desc.createdAt = obj->optValue<SyncTime>("newCreatedAt", 0);
        desc.lastModifiedAt = obj->optValue<SyncTime>("newLastModifiedAt", 0);
    } else if (typeStr == "Delete") {
        desc.type = OperationType::Delete;

        const std::string pathStr = obj->optValue<std::string>("path", "");
        if (pathStr.empty()) throw OperationsParserException("Delete operation missing 'path'");
        desc.path = Str2Path(pathStr);
        validateRelativePath(desc.path, "path");
    } else if (typeStr == "Move") {
        desc.type = OperationType::Move;

        const std::string fromPathStr = obj->optValue<std::string>("fromPath", "");
        const std::string toPathStr = obj->optValue<std::string>("toPath", "");
        if (fromPathStr.empty() || toPathStr.empty()) {
            throw OperationsParserException("Move operation missing 'fromPath' or 'toPath'");
        }
        desc.fromPath = Str2Path(fromPathStr);
        desc.toPath = Str2Path(toPathStr);
        validateRelativePath(desc.fromPath, "fromPath");
        validateRelativePath(desc.toPath, "toPath");
    } else {
        throw OperationsParserException("Unknown operation type: " + typeStr);
    }

    return desc;
}

void ExecuteOperations::applyOperation(const ReplicaSide side, const OperationDesc &desc) {
    switch (side) {
        case ReplicaSide::Local: {
            switch (desc.type) {
                case OperationType::Create: applyLocalCreate(desc); break;
                case OperationType::Edit: applyLocalEdit(desc); break;
                case OperationType::Delete: applyLocalDelete(desc); break;
                case OperationType::Move: applyLocalMove(desc); break;
                default:
                    throw OperationsParserException("Unsupported operation type: " + toString(desc.type));
            }
            break;
        }
        case ReplicaSide::Remote: {
            switch (desc.type) {
                case OperationType::Create: applyRemoteCreate(desc); break;
                case OperationType::Edit: applyRemoteEdit(desc); break;
                case OperationType::Delete: applyRemoteDelete(desc); break;
                case OperationType::Move: applyRemoteMove(desc); break;
                default:
                    throw OperationsParserException("Unsupported operation type: " + toString(desc.type));
            }
            break;
        }
        default:
            throw OperationsParserException("Unsupported side: " + toString(side));
    }
}

void ExecuteOperations::checkExitInfo(const ExitInfo &exitInfo, const std::string &context) {
    if (!exitInfo) {
        throw OperationsParserException(context + " failed: " + std::string(exitInfo));
    }
}

void ExecuteOperations::applyLocalCreate(const OperationDesc &desc) const {
    const SyncPath fullPath = _syncPal->localPath() / desc.path;
    IoError ioError = IoError::Success;
    if (desc.itemType == NodeType::Directory) {
        auto job = std::make_shared<LocalCreateDirJob>(fullPath);
        checkExitInfo(job->runSynchronously(), "Create operation (directory)");
    } else {
        (void) IoHelper::createDirectory(fullPath.parent_path(), true, ioError);
        if (ioError != IoError::Success && ioError != IoError::DirectoryExists) {
            throw OperationsParserException("Create operation (file): unable to create parent directory for '" +
                                             fullPath.string() + "'");
        }
        testhelpers::generateTestFile(fullPath, static_cast<uint64_t>(desc.size));
    }
}

void ExecuteOperations::applyLocalEdit(const OperationDesc &desc) const {
    const SyncPath fullPath = _syncPal->localPath() / desc.path;
    testhelpers::setTestFileSize(fullPath, static_cast<uint64_t>(desc.size));
    if (const IoError ioError = IoHelper::setFileDates(fullPath, desc.createdAt, desc.lastModifiedAt, false);
        ioError != IoError::Success) {
        throw OperationsParserException("Edit operation: unable to set file dates for '" + fullPath.string() + "'");
    }
}

void ExecuteOperations::applyLocalDelete(const OperationDesc &desc) const {
    const SyncPath fullPath = _syncPal->localPath() / desc.path;
    GenericLocalDeleteJob deleteJob(fullPath);
    checkExitInfo(deleteJob.runSynchronously(), "Delete operation");
}

void ExecuteOperations::applyLocalMove(const OperationDesc &desc) const {
    const SyncPath fullFromPath = _syncPal->localPath() / desc.fromPath;
    const SyncPath fullToPath = _syncPal->localPath() / desc.toPath;
    LocalMoveJob job(fullFromPath, fullToPath);
    checkExitInfo(job.runSynchronously(), "Move operation");
}

NodeId ExecuteOperations::remoteIdForPath(const SyncPath &path, const std::string &context) const {
    if (const auto it = _batchRemoteIds.find(path); it != _batchRemoteIds.end()) {
        return it->second;
    }

    bool found = false;
    std::optional<NodeId> id;
    if (!_syncPal->syncDb()->id(ReplicaSide::Remote, path, id, found) || !found || !id) {
        throw OperationsParserException(context + ": remote item not found for " + path.string());
    }
    return *id;
}

void ExecuteOperations::applyRemoteCreate(const OperationDesc &desc) {
    const NodeId parentId = remoteIdForPath(desc.path.parent_path(), "Create operation");

    if (desc.itemType == NodeType::Directory) {
        CreateDirJob job(nullptr, _syncPal->driveDbId(), parentId, desc.path.filename().native());
        checkExitInfo(job.runSynchronously(), "Create operation (directory)");
        _batchRemoteIds[desc.path] = job.nodeId();
    } else {
        // This is a remote-only operation: the file doesn't exist locally, but UploadJob needs to read its
        // content from a local path. Generate it in a temporary location, upload it, then remove it so the
        // local replica is left untouched by this remote-only operation.
        const LocalTemporaryDirectory temporaryDir("executeRemoteOperations");
        const SyncPath fullPath = temporaryDir.path() / desc.path.filename();
        testhelpers::generateTestFile(fullPath, static_cast<uint64_t>(desc.size));
        if (const IoError ioError = IoHelper::setFileDates(fullPath, desc.createdAt, desc.lastModifiedAt, false);
            ioError != IoError::Success) {
            throw OperationsParserException("Create operation (file upload): unable to set file dates for '" +
                                             fullPath.string() + "'");
        }

        UploadJob job(nullptr, _syncPal->driveDbId(), fullPath, desc.path.filename().native(), parentId, desc.createdAt,
                      desc.lastModifiedAt);
        checkExitInfo(job.runSynchronously(), "Create operation (file upload)");
        _batchRemoteIds[desc.path] = job.nodeId();
    }
}

void ExecuteOperations::applyRemoteEdit(const OperationDesc &desc) const {
    const NodeId fileId = remoteIdForPath(desc.path, "Edit operation");

    const SyncPath fullPath = _syncPal->localPath() / desc.path;
    UploadJob job(nullptr, _syncPal->driveDbId(), fullPath, fileId, desc.lastModifiedAt);
    checkExitInfo(job.runSynchronously(), "Edit operation (upload)");
}

void ExecuteOperations::applyRemoteDelete(const OperationDesc &desc) {
    const NodeId itemId = remoteIdForPath(desc.path, "Delete operation");

    DeleteJob job(_syncPal->driveDbId(), itemId);
    job.setBypassCheck(true);
    checkExitInfo(job.runSynchronously(), "Delete operation");
    _batchRemoteIds.erase(desc.path);
}

void ExecuteOperations::applyRemoteMove(const OperationDesc &desc) {
    const NodeId itemId = remoteIdForPath(desc.fromPath, "Move operation");

    if (desc.fromPath.parent_path() == desc.toPath.parent_path()) {
        // Same parent: rename only.
        const SyncPath fullToPath = _syncPal->localPath() / desc.toPath;
        RenameJob job(nullptr, _syncPal->driveDbId(), itemId, fullToPath);
        checkExitInfo(job.runSynchronously(), "Move operation (rename)");
    } else {
        const NodeId destParentId = remoteIdForPath(desc.toPath.parent_path(), "Move operation: destination parent");

        const SyncPath fullToPath = _syncPal->localPath() / desc.toPath;
        MoveJob job(nullptr, _syncPal->driveDbId(), fullToPath, itemId, destParentId, desc.toPath.filename().native());
        job.setBypassCheck(true);
        checkExitInfo(job.runSynchronously(), "Move operation");
    }
    _batchRemoteIds.erase(desc.fromPath);
    _batchRemoteIds[desc.toPath] = itemId;
}

} // namespace KDC
