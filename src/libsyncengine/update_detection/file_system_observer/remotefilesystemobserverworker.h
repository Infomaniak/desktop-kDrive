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

#include "filesystemobserverworker.h"
#include "jobs/network/networkjobsparams.h"
#include "update_detection/file_system_observer/snapshot/snapshotitem.h"

#include <Poco/JSON/Object.h>

namespace KDC {

class CsvFullFileListWithCursorJob;

namespace sentry::pTraces::counterScoped {
struct RFSOExploreItem;
}

class RemoteFileSystemObserverWorker : public FileSystemObserverWorker {
    public:
        RemoteFileSystemObserverWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName);
        ~RemoteFileSystemObserverWorker() override;

    protected:
        void execute() override;
        virtual ExitInfo sendLongPoll(const RemoteNodeId &remoteDirId, bool &changes);
        ExitInfo generateInitialSnapshot() override;

    private:
        ExitInfo processEvents(const NodeId &remoteDirId = {}) override;
        [[nodiscard]] ReplicaSide getSnapshotType() const override { return ReplicaSide::Remote; }

        ExitInfo initWithCursor();
        ExitInfo exploreDirectory(const NodeId &nodeId);

        struct ParsingIterationState {
                bool error{false};
                bool ignore{false};
                bool eof{false};
                uint64_t itemCount{0};
        };
        ExitInfo handleSnapshotItem(const SnapshotItem &item, SyncNameSet &existingFiles, ParsingIterationState &iterationState,
                                    sentry::pTraces::counterScoped::RFSOExploreItem &perfMonitor);
        ExitInfo getItemsInDir(const NodeId &dirId, bool saveCursor);

        struct ActionInfo {
                ActionCode actionCode{ActionCode::ActionCodeUnknown};
                SnapshotItem snapshotItem;
                const SyncName &path() const { return _path; };
                void setPath(const SyncName &remotePath);

            private:
                SyncName _path;
        };
        ExitInfo processActions(Poco::JSON::Array::Ptr filesArray);
        ExitInfo extractActionInfo(Poco::JSON::Object::Ptr actionObj, ActionInfo &actionInfo);
        using MoveItemMap = std::unordered_map<NodeId, ActionCode, StringHashFunction, std::equal_to<>>;
        ExitInfo processAction(ActionInfo &actionInfo, MoveItemMap &movedItems);
        void keepTrackOfMovedItem(const ActionInfo &actionInfo, MoveItemMap &movedItems) const;
        bool isDirectoryExplorationRequired(const ActionInfo &actionInfo, const MoveItemMap &movedItems) const;
        ExitInfo removeItemFromSnapshot(const NodeId &id);

        ExitInfo checkRightsAndUpdateItem(const NodeId &nodeId, bool &hasRights, SnapshotItem &snapshotItem);

        ExitInfo checkForUnsupportedCharacters(const SyncName &name, const NodeId &nodeId, NodeType type);

        void countListingRequests();
        void deleteOrphans();
        using RemoteNodeId = NodeId;
        std::vector<RemoteNodeId> getMainDirectoriesRemoteIds() const;

        DriveDbId _driveDbId = -1;

        using CursorMap = std::unordered_map<NodeId, Cursor, StringHashFunction, std::equal_to<>>;
        // Map tracking the cursors of the listing requests made for folders specified by their remote IDs.
        CursorMap _listingCursorMap;
        ExitInfo listingCursor(const NodeId &remoteDirId, Cursor &cursor, TimeStamp timeStamp);
        ExitInfo saveListingCursor(const NodeId &remoteDirId, const Cursor &cursor, const TimeStamp timeStamp);

        NodeSet _blackList; // A list of user-selected folders not to be synchronized.
        int _listingFullCounter = 0;
        std::chrono::steady_clock::time_point _listingFullTimer = std::chrono::steady_clock::now();

        friend class TestRemoteFileSystemObserverWorker;
};

} // namespace KDC
