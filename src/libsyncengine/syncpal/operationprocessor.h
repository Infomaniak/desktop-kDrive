/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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

#include "syncpal/syncpal.h"
#include "syncpal/isyncworker.h"

namespace KDC {

class OperationProcessor : public ISyncWorker {
    public:
        OperationProcessor(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName,
                           bool useSyncDbCache = true);

    protected:
        // Returns false if only non-synced file attributes(e.g., creation date) have changed. Otherwise, returns true.
        ExitInfo editChangeShouldBePropagated(std::shared_ptr<Node> affectedNode, bool &result);

        bool isPseudoConflict(std::shared_ptr<Node> node, std::shared_ptr<Node> correspondingNode);
        /**
         * Find the corresponding node in other tree.
         * First look in DB.
         * If not found in DB, try to find it based on path in other tree.
         * @param node a shared pointer to the node in current tree.
         * @return a shared pointer to the node in other tree. nullptr il not found.
         */
        virtual std::shared_ptr<Node> correspondingNodeInOtherTree(std::shared_ptr<Node> node);
        /**
         * Find the corresponding node in other tree.
         * Looks in DB only.
         * @param node a shared pointer to the node in current tree.
         * @return a shared pointer to the node in other tree. nullptr il not found.
         */
        std::shared_ptr<Node> correspondingNodeDirect(std::shared_ptr<Node> node);

        /**
         *
         * @param a a shared pointer to a node.
         * @param b a shared pointer to another node.
         * @return true if node 'a' is a child of 'b'.
         */
        bool isABelowB(std::shared_ptr<Node> a, std::shared_ptr<Node> b);

    private:
        /**
         * Try to find a corresponding node in other tree based on path
         * @param node a shared pointer to the node in current tree.
         * @return a shared pointer to the node in other tree. nullptr if not found.
         */
        std::shared_ptr<Node> findCorrespondingNodeFromPath(std::shared_ptr<Node> node);

        bool _useSyncDbCache;
};

} // namespace KDC
