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

#include <Poco/JSON/Array.h>
#include <Poco/JSON/Object.h>

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace KDC {
class SyncPal;

class SituationGeneratorException final : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
};

/**
 * @brief Wraps a JSON description of a local or remote directory situation.
 * See SetInitialSituation for the two supported JSON formats.
 */
class Situation {
    public:
        using StringType = std::filesystem::path::string_type;

        explicit Situation(const StringType &jsonDescription);

        // Reads the JSON from a file instead of an inline string. Throws the same way the
        // constructor does if the content isn't valid.
        [[nodiscard]] static Situation fromFile(const std::filesystem::path &filePath);

        const StringType &json() const noexcept;

        bool operator==(const Situation &other) const noexcept = default;

        void log() const;

    private:
        StringType _jsonDescription;
};

/**
 * @brief This class is the single entry point for setting up and driving Syncpal-based tests. It combines:
 *  - Building an initial local/remote filesystem situation from a JSON description (formerly SituationGenerator).
 *  - Applying simple local/remote operations (delete/edit) on top of that situation (formerly ExecuteOperations).
 *
 * Items are created using real local filesystem operations and real remote API jobs, exactly like ExecuteOperations
 * does for operations applied afterwards. This class never touches the SyncDb, update trees, or snapshots directly:
 * it is up to the caller to run a real sync pass (e.g. SyncpalTestHelper::executeSyncUntilEnd) afterwards so that the
 * SyncPal discovers the generated items itself and populates its own Db/update-trees/snapshots with real ids.
 *
 * Two JSON formats are supported for `run` / `generateInitialSituation`:
 *
 * Legacy format: the key is the node ID (lowercase), object values are directories, non-object values are files,
 * and names are automatically set to the uppercase version of the ID:
 * {
 *    "a": { "aa": { "aaa": 1 } },
 *    "b": {},
 *    "c": 1
 * }
 *
 * Extended format: an array under the "content" key allows explicit control over name, type and size.
 * The node ID is derived from toLower(name):
 * {
 *    "content": [
 *        { "type": "Directory", "name": "A", "content": [
 *            { "type": "Directory", "name": "AA", "content": [
 *                { "type": "File", "name": "AAA" }
 *            ]}
 *        ]},
 *        { "type": "Directory", "name": "B" },
 *        { "type": "File",      "name": "C", "size": 1234 }
 *    ]
 * }
 *
 * Both examples generate:
 * .
 * ├── A
 * │   └── AA
 * │       └── AAA
 * ├── B
 * └── C
 *
 * where leaf values / "File" types are files, and the other nodes are directories.
 */
class SetInitialSituation {
    public:
        SetInitialSituation() = default;
        explicit SetInitialSituation(std::shared_ptr<SyncPal> syncPal);

        // JSON = same format documented on the class.
        // Constructs a Situation from jsonDescription (which validates it) and builds the initial situation
        // against the SyncPal passed to the constructor (or set via setSyncpal).
        // returns false if invalid
        bool run(const std::string &jsonDescription);

        void setSyncpal(std::shared_ptr<SyncPal> syncPal);

        [[nodiscard]] const NodeId &remoteRootId() const { return _remoteRootId; }
        void setRemoteDrive(DriveDbId driveDbId, const NodeId &parentRemoteNodeId);

        void generateInitialSituation(const Situation &situation);

    private:
        struct ItemDesc {
                NodeType type = NodeType::File;
                NodeId id; // lowercase, used to derive the relative path and as a map key
                SyncName name; // display name
                int64_t size = 0;
        };

        void addItem(Poco::JSON::Object::Ptr obj, const std::string &parentId = {});
        void addItem(Poco::JSON::Array::Ptr arr, const std::string &parentId);
        void addItem(const ItemDesc &desc, const std::string &parentId);

        void insertLocalItem(const ItemDesc &desc, const NodeId &parentId);
        void insertRemoteItem(const ItemDesc &desc, const NodeId &parentId);

        std::shared_ptr<SyncPal> _syncPal;

        NodeId _remoteRootId;
        std::optional<DriveDbId> _remoteDriveDbId;
        std::unordered_map<NodeId, SyncPath, StringHashFunction, std::equal_to<>>
                _localItemPaths; // item id (lowercase) -> local relative path
        std::unordered_map<NodeId, NodeId, StringHashFunction, std::equal_to<>>
                _remoteNodeIds; // item id (lowercase) -> real remote NodeId
};

} // namespace KDC
