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

#include "syncpal/syncpal.h"
#include "syncpal/isyncworker.h"

namespace KDC {

class OperationProcessor : public ISyncWorker {
    public:
        OperationProcessor(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName);

    protected:
        bool isPseudoConflict(std::shared_ptr<Node> node, std::shared_ptr<Node> correspondingNode);
        std::shared_ptr<Node> correspondingNodeInOtherTree(std::shared_ptr<Node> node, bool useLocalPath = false);
        std::shared_ptr<Node> correspondingNodeDirect(std::shared_ptr<Node> node);
        bool isABelowB(std::shared_ptr<Node> a, std::shared_ptr<Node> b);
};

}  // namespace KDC
