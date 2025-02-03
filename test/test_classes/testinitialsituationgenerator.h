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

#pragma once

#include "db/dbnode.h"
#include "update_detection/update_detector/node.h"
#include "utility/types.h"

#include <Poco/JSON/Object.h>

namespace KDC {

class Node;
class SyncPal;

/**
 * @brief This class aims to provide a simple and efficient way to generate a data structure (Db and update trees) for testing.
 * The structure is provided as a JSON file. Only the ID in lowercase must be provided in the JSON file. The name will be, by
 * default, the same as the ID but in uppercase. For example, the following JSON :
 * {
 *    "a":
 *    {
 *        "aa":
 *        {
 *            "aaa": 1
 *        }
 *    },
 *    "b": 0,
 *    "c": 0
 * }
 * will generate this tree:
 * .
 * ├── A
 * │   └── AA
 * │       └── AAA
 * ├── B
 * └── C
 *
 * where "AAA" is a file, and the other nodes are directories.
 */
class TestInitialSituationGenerator {
    public:
        TestInitialSituationGenerator() = default;
        explicit TestInitialSituationGenerator(const std::shared_ptr<SyncPal> &syncpal) : _syncpal(syncpal) {}

        void setSyncpal(const std::shared_ptr<SyncPal> &syncpal) { _syncpal = syncpal; }
        void generateInitialSituation(const std::string &jsonInputStr);

        [[nodiscard]] std::shared_ptr<Node> getNode(ReplicaSide side, const NodeId &rawId) const;
        bool getDbNode(const NodeId &rawId, DbNode &dbNode) const;

        /**
         * @brief Insert a new node in the update tree.
         * @param side Replica side for the update tree (Local or Remote).
         * @param itemType Directory or File.
         * @param rawId File ID in lowercase.
         * @param rawParentId Parent file ID in lowercase. If empty, the parent is the root node.
         */
        void insertInUpdateTree(ReplicaSide side, NodeType itemType, const NodeId &rawId, const NodeId &rawParentId) const;
        void removeFromUpdateTree(ReplicaSide side, const NodeId &rawId) const;

    private:
        [[nodiscard]] NodeId generateId(ReplicaSide side, const NodeId &rawId) const;

        void addItem(Poco::JSON::Object::Ptr obj, const std::string &rawParentId = {});
        void addItem(NodeType itemType, const std::string &rawId, const std::string &rawParentId) const;

        void insertInDb(NodeType itemType, const NodeId &rawId, const NodeId &rawParentId) const;
        void insertInAllUpdateTrees(NodeType itemType, const NodeId &rawId, const NodeId &rawParentId) const;

        std::shared_ptr<SyncPal> _syncpal;
};

} // namespace KDC
