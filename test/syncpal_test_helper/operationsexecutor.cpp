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

#include "OperationsExecutor.h"

#include "syncpal/syncpal.h"
#include "update_detection/file_system_observer/filesystemobserverworker.h"
#include "test_utility/testhelpers.h"
#include "test_utility/localtemporarydirectory.h"

#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommon/utility/utility.h"

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
#include <Poco/JSON/Stringifier.h>

#include <fstream>
#include <sstream>

namespace KDC {

//
// ─────────────────────────────────────────────────
// Operations
// ─────────────────────────────────────────────────
//

Operations::Operations(const StringType &jsonDescription) {
    try {
        Poco::JSON::Parser parser;
        _jsonObject = parser.parse(SyncName2Str(jsonDescription)).extract<Poco::JSON::Object::Ptr>();
    } catch (Poco::Exception &) {
        throw OperationsParserException("Invalid Operations JSON");
    }

    if (!_jsonObject->has("operations") || !_jsonObject->isArray("operations")) {
        throw OperationsParserException("Operations must contain an 'operations' array");
    }
    _operationsArray = _jsonObject->getArray("operations");
}

Operations Operations::fromFile(const std::filesystem::path &filePath) {
    const std::ifstream file(filePath, std::ios::binary);
    if (!file) throw OperationsParserException("Operations::fromFile: unable to open file: " + filePath.string());

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return Operations(Str2SyncName(buffer.str()));
}

const Poco::JSON::Array::Ptr &Operations::operationsArray() const noexcept {
    return _operationsArray;
}

void Operations::log() const {
    std::ostringstream oss;
    Poco::JSON::Stringifier::stringify(_jsonObject, oss, 2);
    LOGW_INFO(Log::instance()->getLogger(), CommonUtility::s2ws(oss.str()));
}

//
// ─────────────────────────────────────────────────
// OperationsExecutor
// ─────────────────────────────────────────────────
//

OperationsExecutor::OperationsExecutor(const std::shared_ptr<SyncPal> syncPal) :
    _syncPal(syncPal) {}

bool OperationsExecutor::run(const ReplicaSide side, const std::string &jsonDescription) {
    if (!_syncPal) return false;

    try {
        const Operations operations{Str2SyncName(jsonDescription)};
        execute(side, operations);

        return true;
    } catch (const OperationsParserException &) {
        return false;
    }
}

void OperationsExecutor::execute(const ReplicaSide side, const Operations &operations) {
    _batchRemoteIds.clear();
    _remoteOperationsTemporaryDir.reset();

    const auto &arr = operations.operationsArray();
    for (size_t i = 0; i < arr->size(); ++i) {
        const auto &itemObj = arr->getObject(static_cast<uint64_t>(i));
        const OperationDesc desc = parseOperation(itemObj);
        applyOperation(side, desc);
    }

    // Remote operations are applied directly via API jobs, bypassing the SyncPal's own remote polling. Force
    // an immediate refresh so the change is detected right away instead of waiting for the next poll interval,
    // which could otherwise race with (and be missed by) the caller's subsequent executeSyncUntilEnd().
    if (side == ReplicaSide::Remote && _syncPal) {
        _syncPal->_remoteFSObserverWorker->forceUpdate();
    }
}

const SyncPath &OperationsExecutor::remoteOperationsTemporaryDirPath() {
    if (!_remoteOperationsTemporaryDir) {
        _remoteOperationsTemporaryDir.emplace("executeRemoteOperations");
    }
    return _remoteOperationsTemporaryDir->path();
}

void OperationsExecutor::validateRelativePath(const SyncPath &path, const std::string &fieldName) {
    if (path.empty() || path.is_absolute()) {
        throw OperationsParserException("'" + fieldName + "' must be a non-empty relative path: '" + path.string() + "'");
    }
    for (const auto &part: path.lexically_normal()) {
        if (part == "..") {
            throw OperationsParserException("'" + fieldName + "' must not contain '..' components: '" + path.string() + "'");
        }
    }
}

OperationsExecutor::OperationDesc OperationsExecutor::parseOperation(const Poco::JSON::Object::Ptr &obj) {
    if (!obj) throw OperationsParserException("Each operation must be an object");

    static const std::string OPERATION_TYPE_KEY = "type";
    const std::string typeStr = obj->optValue<std::string>(OPERATION_TYPE_KEY, "");

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
    } else if (typeStr == "Edit") {
        desc.type = OperationType::Edit;

        const std::string pathStr = obj->optValue<std::string>("path", "");
        if (pathStr.empty()) throw OperationsParserException("Edit operation missing 'path'");
        desc.path = Str2Path(pathStr);
        validateRelativePath(desc.path, "path");

        desc.size = obj->optValue<int64_t>("newSize", 0);
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

void OperationsExecutor::applyOperation(const ReplicaSide side, const OperationDesc &desc) {
    switch (side) {
        case ReplicaSide::Local: {
            switch (desc.type) {
                case OperationType::Create:
                    applyLocalCreate(desc);
                    break;
                case OperationType::Edit:
                    applyLocalEdit(desc);
                    break;
                case OperationType::Delete:
                    applyLocalDelete(desc);
                    break;
                case OperationType::Move:
                    applyLocalMove(desc);
                    break;
                default:
                    throw OperationsParserException("Unsupported operation type: " + toString(desc.type));
            }
            break;
        }
        case ReplicaSide::Remote: {
            switch (desc.type) {
                case OperationType::Create:
                    applyRemoteCreate(desc);
                    break;
                case OperationType::Edit:
                    applyRemoteEdit(desc);
                    break;
                case OperationType::Delete:
                    applyRemoteDelete(desc);
                    break;
                case OperationType::Move:
                    applyRemoteMove(desc);
                    break;
                default:
                    throw OperationsParserException("Unsupported operation type: " + toString(desc.type));
            }
            break;
        }
        default:
            throw OperationsParserException("Unsupported side: " + toString(side));
    }
}

void OperationsExecutor::checkExitInfo(const ExitInfo &exitInfo, const std::string &context) {
    if (!exitInfo) {
        throw OperationsParserException(context + " failed: " + static_cast<std::string>(exitInfo));
    }
}

void OperationsExecutor::applyLocalCreate(const OperationDesc &desc) const {
    const SyncPath fullPath = _syncPal->localPath() / desc.path;
    IoError ioError = IoError::Success;
    if (desc.itemType == NodeType::Directory) {
        auto job = std::make_shared<LocalCreateDirJob>(fullPath);
        checkExitInfo(job->runSynchronously(), "Create operation (directory)");
        return;
    }
    (void) IoHelper::createDirectory(fullPath.parent_path(), true, ioError);
    if (ioError != IoError::Success && ioError != IoError::DirectoryExists) {
        throw OperationsParserException("Create operation (file): unable to create parent directory for '" +
                                        fullPath.string() + "'");
    }
    testhelpers::generateTestFile(fullPath, static_cast<uint64_t>(desc.size));
}

void OperationsExecutor::applyLocalEdit(const OperationDesc &desc) const {
    const SyncPath fullPath = _syncPal->localPath() / desc.path;
    testhelpers::setTestFileSize(fullPath, static_cast<uint64_t>(desc.size));
}

void OperationsExecutor::applyLocalDelete(const OperationDesc &desc) const {
    const SyncPath fullPath = _syncPal->localPath() / desc.path;
    GenericLocalDeleteJob deleteJob(fullPath);
    checkExitInfo(deleteJob.runSynchronously(), "Delete operation");
}

void OperationsExecutor::applyLocalMove(const OperationDesc &desc) const {
    const SyncPath fullFromPath = _syncPal->localPath() / desc.fromPath;
    const SyncPath fullToPath = _syncPal->localPath() / desc.toPath;
    LocalMoveJob job(fullFromPath, fullToPath);
    checkExitInfo(job.runSynchronously(), "Move operation");
}

NodeId OperationsExecutor::remoteIdForPath(const SyncPath &path, const std::string &context) const {
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

void OperationsExecutor::applyRemoteCreate(const OperationDesc &desc) {
    const NodeId parentId = remoteIdForPath(desc.path.parent_path(), "Create operation");

    if (desc.itemType == NodeType::Directory) {
        CreateDirJob job(nullptr, _syncPal->driveDbId(), parentId, desc.path.filename().native());
        checkExitInfo(job.runSynchronously(), "Create operation (directory)");
        _batchRemoteIds[desc.path] = job.nodeId();
    } else {
        // This is a remote-only operation: the file doesn't exist locally, but UploadJob needs to read its
        // content from a local path. Generate it in the batch's shared temporary location (a random filename
        // avoids collisions with other operations reusing the same directory), upload it, then let the
        // temporary directory be cleaned up once the batch is done, so the local replica is left untouched by
        // this remote-only operation.
        const SyncPath fullPath = remoteOperationsTemporaryDirPath() / CommonUtility::generateRandomStringAlphaNum();
        testhelpers::generateTestFile(fullPath, static_cast<uint64_t>(desc.size));

        // UploadJob requires creation/modification times structurally; no date semantics are relevant here, so
        // the current time is used.
        const auto now = std::time(nullptr);
        UploadJob job(nullptr, _syncPal->driveDbId(), fullPath, desc.path.filename().native(), parentId, now, now);
        checkExitInfo(job.runSynchronously(), "Create operation (file upload)");
        _batchRemoteIds[desc.path] = job.nodeId();
    }
}

void OperationsExecutor::applyRemoteEdit(const OperationDesc &desc) {
    const NodeId fileId = remoteIdForPath(desc.path, "Edit operation");

    // This is a remote-only operation: uploading the unchanged local replica would not reflect the requested
    // newSize. Generate an edited payload in the batch's shared temporary location (a random filename avoids
    // collisions with other operations reusing the same directory), upload that instead, then let the
    // temporary directory be cleaned up once the batch is done, so the local replica is left untouched by this
    // remote-only operation.
    const SyncPath fullPath = remoteOperationsTemporaryDirPath() / CommonUtility::generateRandomStringAlphaNum();
    testhelpers::generateTestFile(fullPath, static_cast<uint64_t>(desc.size));

    // UploadJob requires a modification time structurally; no date semantics are relevant here, so the current
    // time is used.
    UploadJob job(nullptr, _syncPal->driveDbId(), fullPath, fileId, std::time(nullptr));
    checkExitInfo(job.runSynchronously(), "Edit operation (upload)");
}

void OperationsExecutor::applyRemoteDelete(const OperationDesc &desc) {
    const NodeId itemId = remoteIdForPath(desc.path, "Delete operation");

    DeleteJob job(_syncPal->driveDbId(), itemId);
    job.setBypassCheck(true);
    checkExitInfo(job.runSynchronously(), "Delete operation");
    (void) _batchRemoteIds.erase(desc.path);
}

void OperationsExecutor::applyRemoteMove(const OperationDesc &desc) {
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
    (void) _batchRemoteIds.erase(desc.fromPath);
    _batchRemoteIds[desc.toPath] = itemId;
}

} // namespace KDC
