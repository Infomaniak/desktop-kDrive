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

#include "executeoperations.hpp"

#include "test_utility/testhelpers.h"

#include "libcommonserver/log/log.h"

#include <Poco/JSON/Array.h>
#include <Poco/JSON/Parser.h>

namespace KDC {

class OperationsParserException final : public std::runtime_error {
    public:
        explicit OperationsParserException(const std::string &what) :
            std::runtime_error(what) {}
};

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
        const Operations operations(Str2SyncName(jsonDescription));
        executeOperations(side, operations);

        return true;
    } catch (...) {
        return false;
    }
}

void ExecuteOperations::executeOperations(const ReplicaSide side, const Operations &operations) const {
    Poco::JSON::Object::Ptr obj;
    try {
        Poco::JSON::Parser parser;
        obj = parser.parse(SyncName2Str(operations.json())).extract<Poco::JSON::Object::Ptr>();
    } catch (Poco::Exception &) {
        throw OperationsParserException("Invalid JSON input");
    }

    const auto arr = obj->getArray("operations");
    for (size_t i = 0; i < arr->size(); ++i) {
        const auto &itemObj = arr->getObject(static_cast<unsigned int>(i));
        const OperationDesc desc = parseOperation(itemObj);
        applyOperation(side, desc);
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

        desc.size = obj->optValue<int64_t>(
                "size", desc.itemType == NodeType::File ? testhelpers::defaultFileSize : testhelpers::defaultDirSize);
        desc.createdAt = obj->optValue<SyncTime>("createdAt", testhelpers::defaultTime);
        desc.lastModifiedAt = obj->optValue<SyncTime>("lastModifiedAt", testhelpers::defaultTime);
    } else if (typeStr == "Edit") {
        desc.type = OperationType::Edit;

        const std::string pathStr = obj->optValue<std::string>("path", "");
        if (pathStr.empty()) throw OperationsParserException("Edit operation missing 'path'");
        desc.path = Str2Path(pathStr);

        desc.size = obj->optValue<int64_t>("newSize", 0);
        desc.createdAt = obj->optValue<SyncTime>("newCreatedAt", 0);
        desc.lastModifiedAt = obj->optValue<SyncTime>("newLastModifiedAt", 0);
    } else if (typeStr == "Delete") {
        desc.type = OperationType::Delete;

        const std::string pathStr = obj->optValue<std::string>("path", "");
        if (pathStr.empty()) throw OperationsParserException("Delete operation missing 'path'");
        desc.path = Str2Path(pathStr);
    } else if (typeStr == "Move") {
        desc.type = OperationType::Move;

        const std::string fromPathStr = obj->optValue<std::string>("fromPath", "");
        const std::string toPathStr = obj->optValue<std::string>("toPath", "");
        if (fromPathStr.empty() || toPathStr.empty()) {
            throw OperationsParserException("Move operation missing 'fromPath' or 'toPath'");
        }
        desc.fromPath = Str2Path(fromPathStr);
        desc.toPath = Str2Path(toPathStr);
    } else {
        throw OperationsParserException("Unknown operation type: " + typeStr);
    }

    return desc;
}

void ExecuteOperations::applyOperation(const ReplicaSide side, const OperationDesc &desc) const {
    switch (side) {
        case ReplicaSide::Local: {
            switch (desc.type) {
                case OperationType::Create:
                    // TODO: create desc.path (directory or file, see desc.itemType) under _syncPal->localPath(),
                    // e.g. LocalCreateDirJob for a directory, or a plain file write for a file.
                    break;
                case OperationType::Edit:
                    // TODO: edit the local file at _syncPal->localPath() / desc.path (content/size/mtime, see
                    // desc.size / desc.lastModifiedAt).
                    break;
                case OperationType::Delete:
                    // TODO: delete the local item at _syncPal->localPath() / desc.path,
                    // e.g. GenericLocalDeleteJob / SyncLocalDeleteJob.
                    break;
                case OperationType::Move:
                    // TODO: move/rename the local item from _syncPal->localPath() / desc.fromPath to
                    // _syncPal->localPath() / desc.toPath, e.g. LocalMoveJob.
                    break;
                default:
                    throw OperationsParserException("Unsupported operation type: " + toString(desc.type));
            }
            break;
        }
        case ReplicaSide::Remote: {
            switch (desc.type) {
                case OperationType::Create:
                    // TODO: create desc.path (directory or file, see desc.itemType) on drive _syncPal->driveDbId(),
                    // e.g. CreateDirJob for a directory, or UploadJob for a file.
                    break;
                case OperationType::Edit:
                    // TODO: edit the remote file at desc.path on drive _syncPal->driveDbId(),
                    // e.g. UploadJob (file-id overload).
                    break;
                case OperationType::Delete:
                    // TODO: delete the remote item at desc.path on drive _syncPal->driveDbId(), e.g. DeleteJob.
                    break;
                case OperationType::Move:
                    // TODO: move/rename the remote item from desc.fromPath to desc.toPath on drive _syncPal->driveDbId(),
                    // e.g. MoveJob / RenameJob.
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

} // namespace KDC
