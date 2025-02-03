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

#include "utility/types.h"

#include <Poco/JSON/Object.h>

namespace KDC {

class SyncPal;

/**
 * @brief This class aims to provide a simple and efficient way to generate a data structure (Db and update trees) for testing.
 * The structure is provided as a JSON file. For example, the following JSON :
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
        explicit TestInitialSituationGenerator(const std::shared_ptr<SyncPal> &syncpal) : _syncpal(syncpal) {}

        void generateInitialSituation(const std::string &jsonInputStr);

        static NodeId generateId(ReplicaSide side, const NodeId &rawId);

    private:
        void addItem(Poco::JSON::Object::Ptr obj, const std::string &parentId = {});
        void addItem(NodeType itemType, const std::string &id, const std::string &parentId);

        void insertInDb(NodeType itemType, const NodeId &id, const NodeId &parentId) const;
        void insertInUpdateTrees(NodeType itemType, const NodeId &id, const NodeId &parentId);
        void insertInUpdateTrees(ReplicaSide side, NodeType itemType, const NodeId &id, const NodeId &parentId) const;

        std::shared_ptr<SyncPal> _syncpal;
};

} // namespace KDC
