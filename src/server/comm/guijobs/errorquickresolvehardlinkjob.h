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

// This job is used to quickly resolve a HardlinkNotSupportedError. Such errors occur on Windows when a file is hardlinked into
// another sync root (e.g. OneDrive) as the sync engine does not support hardlinks shared between two sync roots.
// The job removes the node from the sync database, saves a copy of the file into the rescue folder if the local file is not in
// sync with the database, and hard removes all the links of the file located under the sync root. The next synchronization will
// see the remote file as a new item and will download it again as a standard file.
class ErrorQuickResolveHardlinkJob : public AbstractGuiJob {
    public:
        ErrorQuickResolveHardlinkJob(std::shared_ptr<CommManager> commManager, int32_t requestId,
                                     const Poco::DynamicStruct &inParams, std::shared_ptr<AbstractCommChannel> channel);

    private:
        // Input parameters
        SyncDbId _syncDbId = 0;
        ErrorDbId _errorDbId = 0;
        NodeId _nodeId;
        SyncPath _relativeLocalPath;

        ExitInfo deserializeInputParms() override;
        ExitInfo serializeOutputParms() override { return ExitCode::Ok; }
        ExitInfo process() override;

        ExitInfo quickResolve(const std::shared_ptr<SyncPal> &syncPal) const;

        // Checks that the reported error exists in the parameters database and is the hardlink error of the reported sync, node
        // and path.
        ExitInfo checkParmsDbError() const;
        // Fetches the reported node from the sync database. Only the file nodes are accepted.
        ExitInfo fetchFileDbNode(const std::shared_ptr<SyncPal> &syncPal, DbNode &dbNode) const;
        // Builds the absolute path of the reported item and checks that it is located under the sync root.
        ExitInfo getSeedPath(const SyncPath &localPath, SyncPath &seedPath) const;
        // Checks that the reported item is a regular file referring to the reported node. seedExists is set to false if the item
        // does not exist anymore.
        ExitInfo checkSeedItem(const SyncPath &seedPath, bool &seedExists) const;
        // Retrieves the local node id of the item located at path, without following symbolic links. nodeId is set to
        // std::nullopt if the item does not exist.
        ExitInfo getLocalNodeId(const SyncPath &path, std::optional<NodeId> &nodeId) const;
        // Hard removes all the given links of the reported file located under the sync root, after saving a copy of the file into
        // the rescue folder if it is not in sync with the database. seedPath is one of the links.
        ExitInfo removeLinks(const std::shared_ptr<SyncPal> &syncPal, const DbNode &dbNode, const SyncPath &seedPath,
                             const std::vector<SyncPath> &linkPaths) const;
        // Retrieves all the links of the reported file located under the sync root, including the seed path.
        ExitInfo getLinkPathsUnderSyncRoot(const SyncPath &localPath, const SyncPath &seedPath,
                                           std::vector<SyncPath> &linkPaths) const;
        // Retrieves all the links of the reported file located under the sync root by searching its node id, when the reported
        // path does not exist anymore. linkPaths is empty if the file has no link left under the sync root.
        ExitInfo findLinkPathsByNodeId(const SyncPath &localPath, std::vector<SyncPath> &linkPaths) const;
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
        friend class TestErrorQuickResolveHardlinkJob;
};

} // namespace KDC
