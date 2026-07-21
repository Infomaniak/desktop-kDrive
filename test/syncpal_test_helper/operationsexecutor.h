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

#pragma once

#include "utility/types.h"
#include "test_utility/localtemporarydirectory.h"

#include <Poco/JSON/Array.h>
#include <Poco/JSON/Object.h>

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace KDC {
class SyncPal;

class OperationsParserException final : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
};

/**
 * @brief Wraps a JSON description of a list of local/remote operations to apply on top of an existing
 * situation (see Situation / InitialSituationSetter in initialsituationsetter.h).
 * Supported operation types: Create, Edit, Delete and Move, e.g.:
 * {
 *    "operations": [
 *        { "type": "Edit", "path": "C/D/e", "newSize": 1234 },
 *        { "type": "Create", "itemType": "File", "path": "C/D", "name": "f", "size": 5678 },
 *        { "type": "Delete", "path": "F/G/H" },
 *        { "type": "Move", "fromPath": "I/J/k", "toPath": "L/m" }
 *    ]
 * }
 * Note: for Create, "path" is the parent directory the item is created into (optional, defaults to the sync
 * root) and "name" is the item's own name (not a path).
 */
class Operations {
    public:
        using StringType = std::filesystem::path::string_type;

        explicit Operations(const StringType &jsonDescription); // throws if jsonDescription is not valid

        // Reads the JSON from a file instead of an inline string. Throws the same way the
        // constructor does if the content isn't valid.
        [[nodiscard]] static Operations fromFile(const std::filesystem::path &filePath);

        // Operations are immutable once parsed and not meant to be duplicated: forbid copies (the parsed
        // JSON is only stored once, moves are still allowed).
        Operations(const Operations &) = delete;
        Operations &operator=(const Operations &) = delete;
        Operations(Operations &&) = default;
        Operations &operator=(Operations &&) = default;

        // The parsed "operations" JSON array, ready to be iterated without re-parsing.
        const Poco::JSON::Array::Ptr &operationsArray() const noexcept;

        void log() const;

    private:
        Poco::JSON::Object::Ptr _jsonObject;
        Poco::JSON::Array::Ptr _operationsArray;
};

/**
 * @brief Applies an Operations JSON description (Create/Edit/Delete/Move) on the local or remote replica
 * of the SyncPal passed to the constructor (or set via setSyncpal).
 */
class OperationsExecutor {
    public:
        OperationsExecutor() = default;
        explicit OperationsExecutor(std::shared_ptr<SyncPal> syncPal);

        void setSyncpal(const std::shared_ptr<SyncPal> syncPal) { _syncPal = syncPal; }

        // JSON = same format documented on Operations.
        // Constructs an Operations from jsonDescription (which validates it) and applies it, on the given
        // side, against the SyncPal passed to the constructor (or set via setSyncpal).
        // returns false if invalid
        bool run(ReplicaSide side, const std::string &jsonDescription);

        // Not const: applying operations triggers real local/remote side effects (filesystem changes, network
        // jobs) and tracks per-batch state in `_batchRemoteIds`.
        void execute(ReplicaSide side, const Operations &operations);

    private:
        struct OperationDesc {
                OperationType type = OperationType::None;
                SyncPath path; // Create: resolved as "path" (parent dir) / "name" (item name). Edit / Delete ("path"):
                               // item affected, relative to the sync root.
                SyncPath fromPath; // Move ("fromPath"): source item, relative to the sync root.
                SyncPath toPath; // Move ("toPath"): destination item, relative to the sync root.
                NodeType itemType = NodeType::File; // Create ("itemType"): File or Directory.
                int64_t size = 0; // Create ("size") / Edit ("newSize").
        };

        [[nodiscard]] static OperationDesc parseOperation(const Poco::JSON::Object::Ptr &obj);

        // Ensures `path` is a relative path that stays within the sync root once normalized (i.e. not absolute
        // and without any ".." component that could make it escape). Throws OperationsParserException
        // (with `fieldName` prefixed to the error message) otherwise.
        static void validateRelativePath(const SyncPath &path, const std::string &fieldName);

        // Throws OperationsParserException (with `context` prefixed to the error message) if `exitInfo` does
        // not indicate success. Used to surface local/remote job failures instead of silently ignoring them.
        static void checkExitInfo(const ExitInfo &exitInfo, const std::string &context);

        void applyOperation(ReplicaSide side, const OperationDesc &desc);

        // Local side, one function per operation type.
        void applyLocalCreate(const OperationDesc &desc) const;
        void applyLocalEdit(const OperationDesc &desc) const;
        void applyLocalDelete(const OperationDesc &desc) const;
        void applyLocalMove(const OperationDesc &desc) const;

        // Remote side, one function per operation type.
        void applyRemoteCreate(const OperationDesc &desc);
        void applyRemoteEdit(const OperationDesc &desc);
        void applyRemoteDelete(const OperationDesc &desc);
        void applyRemoteMove(const OperationDesc &desc);

        // Resolves the remote NodeId of `path`, checking items created/moved within the current batch
        // (`_batchRemoteIds`, since they may not be in the sync DB yet) before falling back to the sync DB.
        // Throws OperationsParserException (with `context` prefixed to the error message) if not found.
        [[nodiscard]] NodeId remoteIdForPath(const SyncPath &path, const std::string &context) const;

        std::shared_ptr<SyncPal> _syncPal;

        // Temporary directory used to generate the local payload of remote-only operations (Create/Edit file
        // uploads). Lazily created on first use and reused for the whole OperationsExecutor() batch, instead of
        // creating/destroying one per operation.
        std::optional<LocalTemporaryDirectory> _remoteOperationsTemporaryDir;
        [[nodiscard]] const SyncPath &remoteOperationsTemporaryDirPath();

        // Remote ids of items created/moved during the current OperationsExecutor() batch, keyed by their
        // current relative path. Needed because those items may not exist in the sync DB yet (no sync has
        // run within the batch), so a subsequent operation referencing them (e.g. a Create inside a
        // directory just created earlier in the same batch) cannot rely on a sync DB lookup alone.
        std::map<SyncPath, NodeId> _batchRemoteIds;
};

} // namespace KDC