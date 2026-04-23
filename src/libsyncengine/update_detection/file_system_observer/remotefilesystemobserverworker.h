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
        ExitInfo processEvents(const NodeId &remoteDirId) override;
        [[nodiscard]] ReplicaSide getSnapshotType() const override { return ReplicaSide::Remote; }

        ExitInfo initWithCursor();
        ExitInfo exploreDirectory(const NodeId &nodeId);

        struct ParsingIterationState {
                bool error{false};
                bool ignore{false};
                bool eof{false};
                uint64_t itemCount{0};
        };
        [[nodiscard]] ExitInfo handleSnapshotItem(const SnapshotItem &item, SyncNameSet &existingFiles,
                                                  ParsingIterationState &iterationState,
                                                  sentry::pTraces::counterScoped::RFSOExploreItem &perfMonitor);
        enum class CursorPersistence {
            Save,
            None
        };
        [[nodiscard]] ExitInfo getItemsInDir(const NodeId &dirId, CursorPersistence persistence = CursorPersistence::None);

        struct ActionInfo {
                ActionCode actionCode{ActionCode::ActionCodeUnknown};
                RemoteSnapshotItem snapshotItem;
                const SyncName &path() const { return _path; };
                void setPath(const SyncName &remotePath);

            private:
                SyncName _path;
        };

        using RemoteFileId = int64_t;
        static RemoteFileId toRemoteFileId(const NodeId &nodeId) { return std::stoi(nodeId); };
        using ActionInfoList = std::list<ActionInfo>;
        ExitInfo createActionInfoList(const Poco::JSON::Array::Ptr actionArray, ActionInfoList &actionInfoList);
        ExitInfo fillActionsFilesInfo(const Poco::JSON::Array::Ptr actionsFilesArray, ActionInfoList &actionInfoList);
        ExitInfo extractActionInfo(const Poco::JSON::Object::Ptr actionObj, ActionInfo &actionInfo);
        ExitInfo extractActionFileInfo(const Poco::JSON::Object::Ptr actionFileObj, ActionInfoList &actionInfoList);

        using MoveItemMap = std::unordered_map<NodeId, ActionCode, StringHashFunction, std::equal_to<>>;
        ExitInfo processAction(ActionInfo &actionInfo, MoveItemMap &movedItems);
        ExitInfo processActions(const Poco::JSON::Array::Ptr actionsArray, const Poco::JSON::Array::Ptr actionFilesArray);

        void keepTrackOfMovedItem(const ActionInfo &actionInfo, MoveItemMap &movedItems) const;
        bool isDirectoryExplorationRequired(const ActionInfo &actionInfo, const MoveItemMap &movedItems) const;

        ExitInfo removeItemFromSnapshot(const NodeId &id);

        ExitInfo checkRightsAndUpdateItem(const NodeId &nodeId, bool &hasRights, SnapshotItem &snapshotItem);
        ExitInfo checkForUnsupportedCharacters(const SyncName &name, const NodeId &nodeId, NodeType type);

        void countListingRequests();
        void deleteOrphans();

        ExitInfo getMainDirectoriesRemoteIds(std::vector<RemoteNodeId> &mainDirectoriesRemoteIds) const;

        DriveDbId _driveDbId = -1;

        using CursorMap = std::unordered_map<RemoteNodeId, CursorData, StringHashFunction, std::equal_to<>>;
        // Map tracking the cursors of the listing requests made for folders specified by their remote IDs.
        CursorMap _listingCursorMap;
        ExitInfo getListingCursor(const RemoteNodeId &remoteDirId, CursorData &cursorData);
        ExitInfo saveListingCursor(const RemoteNodeId &remoteDirId, const CursorData &cursorData);

        RemoteNodeIdSet _blackList; // A list of user-selected folders not to be synchronized.
        int _listingFullCounter = 0;
        std::chrono::steady_clock::time_point _listingFullTimer = std::chrono::steady_clock::now();

        ExitInfo updateV3MainFolderItem(const RemoteNodeId &remoteNodeId);
        ExitInfo getV3RemoteFolderName(const RemoteNodeId &remoteDirId, SyncName &folderName);

        friend class TestRemoteFileSystemObserverWorker;
};

} // namespace KDC
