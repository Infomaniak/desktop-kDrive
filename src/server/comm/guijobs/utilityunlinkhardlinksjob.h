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

#include "server/comm/guijobs/abstractguijob.h"
#include "libcommon/utility/types.h"

#include <optional>
#include <vector>

namespace KDC {

class DbNode;
class SyncPal;

// This job is used to unlink the hardlinks of a file reported with a HardlinkNotSupportedError. Such errors occur on Windows
// when a file is hardlinked into
// another sync root (e.g. OneDrive) as the sync engine does not support hardlinks shared between two sync roots.
// The job removes the node from the sync database, saves a copy of the file into the rescue folder if the local file is not in
// sync with the database, and hard removes all the links of the file located under the sync root. The next synchronization will
// see the remote file as a new item and will download it again as a standard file.
class UtilityUnlinkHardlinksJob : public AbstractGuiJob {
    public:
        UtilityUnlinkHardlinksJob(std::shared_ptr<CommManager> commManager, int32_t requestId,
                                  const Poco::DynamicStruct &inParams, std::shared_ptr<AbstractCommChannel> channel);

    private:
        // Input parameters
        SyncDbId _syncDbId = 0;
        ErrorDbId _errorDbId = 0;
        NodeId _nodeId;

        ExitInfo deserializeInputParms() override;
        ExitInfo serializeOutputParms() override { return ExitCode::Ok; }
        ExitInfo process() override;

        ExitInfo unlinkHardlinks(const std::shared_ptr<SyncPal> &syncPal) const;

        // Removes all the links of the reported file located under the sync root, then the node of the file from the sync
        // database. The links are searched by node id.
        ExitInfo removeLinksAndNode(const std::shared_ptr<SyncPal> &syncPal, const DbNode &dbNode) const;

        // Checks that the reported error exists in the parameters database and is the hardlink error of the reported sync and
        // node.
        ExitInfo checkParmsDbError() const;
        // Fetches the reported node from the sync database. Only the file nodes are accepted. nodeFound is set to false if the
        // node is not present in the sync database anymore, in which case the caller only completes the cleanup.
        ExitInfo fetchFileDbNode(const std::shared_ptr<SyncPal> &syncPal, DbNode &dbNode, bool &nodeFound) const;
        // Retrieves the local node id of the item located at path, without following symbolic links. nodeId is set to
        // std::nullopt if the item does not exist.
        ExitInfo getLocalNodeId(const SyncPath &path, std::optional<NodeId> &nodeId) const;
        // Selects the link to use as the seed path among the given links: the current seed path if it still refers to the
        // reported node, or the first link that does. seedFound is set to false if none of the links refers to the reported
        // node anymore.
        ExitInfo selectSeedPath(const std::vector<SyncPath> &linkPaths, SyncPath &seedPath, bool &seedFound) const;
        // Hard removes all the given links of the reported file located under the sync root, after saving a copy of the file into
        // the rescue folder if it is not in sync with the database. seedPath is one of the links and is updated with a link that
        // still refers to the reported node, if the item located at the given seed path has been replaced.
        ExitInfo removeLinks(const std::shared_ptr<SyncPal> &syncPal, const DbNode &dbNode, SyncPath &seedPath,
                             const std::vector<SyncPath> &linkPaths) const;
        // Retrieves all the links of the reported file located under the sync root, searched by node id. linkPaths is empty if
        // the file does not exist anymore or has no link left under the sync root.
        ExitInfo getLinkPathsUnderSyncRoot(const SyncPath &localPath, std::vector<SyncPath> &linkPaths) const;
        // Saves a copy of the reported file into the rescue folder.
        ExitInfo rescueFile(const std::shared_ptr<SyncPal> &syncPal, const DbNode &dbNode, const SyncPath &seedPath) const;
        // Hard removes the links that still refer to the reported node, the seed path last.
        ExitInfo deleteLinks(const std::shared_ptr<SyncPal> &syncPal, const std::vector<SyncPath> &linkPaths,
                             const SyncPath &seedPath) const;
        // Removes the reported node from the sync database.
        ExitInfo deleteDbNode(const std::shared_ptr<SyncPal> &syncPal, const DbNode &dbNode) const;
        // Removes the reported error from the parameters database.
        ExitInfo deleteParmsDbError() const;

        friend class TestGuiCommChannel;
        friend class TestUtilityUnlinkHardlinksJob;
};

} // namespace KDC
