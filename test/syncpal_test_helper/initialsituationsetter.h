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

#include "libcommon/utility/types.h"
#include "utility/types.h"
#include "test_utility/localtemporarydirectory.h"

#include <Poco/JSON/Array.h>
#include <Poco/JSON/Object.h>

#include <filesystem>
#include <memory>
#include <optional>
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
 * See InitialSituationSetter for the two supported JSON formats.
 */
class Situation {
    public:
        explicit Situation(const SyncName &jsonDescription);

        // Reads the JSON from a file instead of an inline string. Throws the same way the
        // constructor does if the content isn't valid.
        [[nodiscard]] static Situation fromFile(const std::filesystem::path &filePath);

        // Situations are immutable once parsed and not meant to be duplicated: forbid copies (the parsed
        // JSON is only stored once, moves are still allowed).
        Situation(const Situation &) = delete;
        Situation &operator=(const Situation &) = delete;
        Situation(Situation &&) = default;
        Situation &operator=(Situation &&) = default;

        // The parsed JSON object, ready to be consumed without re-parsing.
        const Poco::JSON::Object::Ptr &jsonObject() const noexcept;

        bool operator==(const Situation &other) const noexcept;

        void log() const;

    private:
        Poco::JSON::Object::Ptr _jsonObject;
};

/**
 * @brief This class is the single entry point for setting up and driving Syncpal-based tests. It combines:
 *  - Building an initial local/remote filesystem situation from a JSON description (formerly SituationGenerator).
 *  - Applying simple local/remote operations (delete/edit) on top of that situation (formerly OperationsExecutor).
 *
 * Items are created using real local filesystem operations and real remote API jobs, exactly like OperationsExecutor
 * does for operations applied afterwards. This class never touches the SyncDb, update trees, or snapshots directly:
 * it is up to the caller to run a real sync pass (e.g. SyncpalTestHelper::executeSyncUntilEnd) afterwards so that the
 * SyncPal discovers the generated items itself and populates its own Db/update-trees/snapshots with real ids.
 *
 * `run` / `generateInitialSituation` take one Situation for the local side and one for the remote side, so
 * differing (e.g. conflicting or asymmetrical) initial situations can be set up on each side independently.
 *
 * Two JSON formats are supported for each side's description:
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
class InitialSituationSetter {
    public:
        InitialSituationSetter() = default;
        explicit InitialSituationSetter(std::shared_ptr<SyncPal> syncPal);

        // JSON = same format documented on the class.
        // Constructs a Situation for each side from localJsonDescription/remoteJsonDescription (which validates
        // them) and builds the initial situation against the SyncPal passed to the constructor (or set via
        // setSyncpal). Either description may describe an empty situation if that side shouldn't be populated.
        // returns false if invalid
        bool run(const std::string &localJsonDescription, const std::string &remoteJsonDescription);

        void setSyncpal(std::shared_ptr<SyncPal> syncPal);

        // Builds the local and remote situations independently, so that different content can be requested on
        // each side (e.g. to set up conflicting or asymmetrical initial states). Either situation may be left
        // empty (no "content" and no keys) if that side shouldn't be populated.
        void generateInitialSituation(const Situation &localSituation, const Situation &remoteSituation);

    private:
        struct ItemDesc {
                NodeType type = NodeType::File;
                NodeId id; // lowercase, used to derive the relative path and as a map key
                SyncName name; // display name
                int64_t size = 0;
        };

        // side: Local -> creates real filesystem items only. Remote -> creates real remote API items only.
        void generateSituation(const Situation &situation, ReplicaSide side);

        void addItem(ReplicaSide side, Poco::JSON::Object::Ptr obj, const std::string &parentId = {});
        void addItem(ReplicaSide side, Poco::JSON::Array::Ptr arr, const std::string &parentId);
        void addItem(ReplicaSide side, const ItemDesc &desc, const std::string &parentId);

        void insertLocalItem(const ItemDesc &desc, const NodeId &parentId);
        void insertRemoteItem(const ItemDesc &desc, const NodeId &parentId);

        // Returns the local path to upload from for a remote file item: the real generated local item if one
        // exists at the same id (both sides describe it), otherwise a scratch file generated on the fly in a
        // dedicated temporary directory (lazily created), for remote-only situations.
        SyncPath localFilePathForUpload(const ItemDesc &desc);

        std::shared_ptr<SyncPal> _syncPal;

        std::unordered_map<NodeId, SyncPath, StringHashFunction, std::equal_to<>>
                _localItemPaths; // item id (lowercase) -> local relative path
        std::unordered_map<NodeId, NodeId, StringHashFunction, std::equal_to<>>
                _remoteNodeIds; // item id (lowercase) -> real remote NodeId
        std::optional<LocalTemporaryDirectory> _uploadScratchDir; // used only for remote-only file items
};

} // namespace KDC
