/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#include "syncpal/operationprocessor.h"
#include "syncpal/syncpal.h"

#include <list>
#include <queue>

namespace KDC {

class UpdateTree;

class OperationGeneratorWorker : public OperationProcessor {
    public:
        OperationGeneratorWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName);

    protected:
        virtual void execute() override;

    private:
        void generateCreateOperation(std::shared_ptr<Node> currentNode, std::shared_ptr<Node> correspondingNode);
        void generateEditOperation(std::shared_ptr<Node> currentNode, std::shared_ptr<Node> correspondingNode);
        void generateMoveOperation(std::shared_ptr<Node> currentNode, std::shared_ptr<Node> correspondingNode);
        void generateDeleteOperation(std::shared_ptr<Node> currentNode, std::shared_ptr<Node> correspondingNode);

        void findAndMarkAllChildNodes(std::shared_ptr<Node> parentNode);

        std::queue<std::shared_ptr<Node>> _queuedToExplore;
        std::unordered_set<NodeId> _deletedNodes;

        int64_t _bytesToDownload = 0;

        friend class TestOperationGeneratorWorker;
};

}  // namespace KDC
