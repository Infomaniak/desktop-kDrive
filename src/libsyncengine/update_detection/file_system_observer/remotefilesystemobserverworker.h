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

#include "filesystemobserverworker.h"
#include "jobs/network/networkjobsparams.h"
#include "update_detection/file_system_observer/snapshot/snapshotitem.h"

#include <Poco/JSON/Object.h>
#include <atomic>

#include <optional>

namespace KDC {

class ContinueFileListWithCursorJob;
class CsvFullFileListWithCursorJob;
class LongPollJob;

namespace sentry::pTraces::counterScoped {
struct RFSOExploreItem;
}

class RemoteFileSystemObserverWorker : public FileSystemObserverWorker {
    public:
        RemoteFileSystemObserverWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName);
        ~RemoteFileSystemObserverWorker() override;

        Count listingFullCount() const { return _listingFullCount.load(); };
        RemoteNodeIdSet blackList() const;
        void setBlackList(RemoteNodeIdSet blackList);
        void restoreRemoteSnapshotBackup(const ConstSnapshot &snapshotBackup);
        [[nodiscard]] ExitInfo checkIfValidListingSyncFolderCursorExists(bool &validSnapshotBackupExists) const;

    protected:
        void execute() override;
        // Unlike the `init` method of the parent class, this method:
        // - does not set the init flag to `true`, which would otherwise have the effect of skipping `longpoll` and
        // `listing/continue` requests,
        // - does not invalidate the remote snapshot (which would otherwise trigger the full reconstruction of the remote
        // snapshot),
        // - does set the updating flag to `true` in order to trigger the `longpoll` and `listing/continue` requests.
        void init() override;
        ExitInfo generateInitialSnapshot() override;
        enum class ForcedUpdate {
            Asked,
            None
        };
        [[nodiscard]] virtual ExitInfo checkIfRemoteDirHasChanges(const RemoteNodeId &remoteDirId, ForcedUpdate updateFlag,
                                                                  const std::shared_ptr<LongPollJob> &longPollJob, bool &changes);

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

        [[nodiscard]] ExitInfo insertItemInRemoteSnapshot(const RemoteSnapshotItem &item, SyncNameSet &existingFiles);
        [[nodiscard]] ExitInfo handleRemoteSnapshotItem(const RemoteSnapshotItem &item, SyncNameSet &existingFiles,
                                                        ParsingIterationState &iterationState,
                                                        sentry::pTraces::counterScoped::RFSOExploreItem &perfMonitor);
        enum class CursorPersistence {
            Save,
            None
        };
        [[nodiscard]] ExitInfo getItemsInRemoteDir(const NodeId &dirId, CursorPersistence persistence = CursorPersistence::None);

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
        [[nodiscard]] ExitInfo deleteOrphans();

        DriveDbId _driveDbId = -1;

        // Cache of the listing cursor of the sync folder.
        mutable std::optional<CursorData> _listingSyncFolderCursorCache;
        mutable std::mutex _listingSyncFolderCursorCacheMutex;
        [[nodiscard]] ExitInfo getSyncFolderListingCursor(CursorData &cursorData) const;
        [[nodiscard]] ExitInfo setSyncFolderListingCursor(const CursorData &cursorData);

        // Cache of the listing cursor of the shared folder.
        mutable std::optional<CursorData> _sharedFolderListingCursorCache;
        mutable std::mutex _sharedFolderListingCursorCacheMutex;
        [[nodiscard]] ExitInfo getSharedFolderListingCursor(CursorData &cursorData) const;
        [[nodiscard]] ExitInfo setSharedFolderListingCursor(const CursorData &cursorData);

        /**
         * @brief Clears the listing cursor of the sync folder.
         *
         * This function resets the state of the listing cursor, effectively removing any previously saved cursor data
         * for the sync folder. It ensures that subsequent listing operations start without relying on outdated
         * cursor data.
         *
         * @return ExitInfo indicating the success or failure of the operation.
         */
        [[nodiscard]] ExitInfo clearSyncFolderListingCursor();

        /**
         * @brief Clears the listing cursor of the shared folder.
         *
         * This function resets the state of the shared folder listing cursor, effectively removing any previously saved
         * cursor data for the shared folder. It ensures that subsequent listing operations start without relying on
         * outdated cursor data.
         *
         * @return ExitInfo indicating the success or failure of the operation.
         */
        [[nodiscard]] ExitInfo clearSharedFolderListingCursor();

        RemoteNodeIdSet _blackList; // A list of user-selected folders not to be synchronized.
        mutable std::mutex _blackListMutex; // Mutex to protect access to the _blackList member variable.
        std::atomic<Count> _listingFullCount{0}; // Thread-safe counter for full listing requests
        std::chrono::steady_clock::time_point _listingFullTimer = std::chrono::steady_clock::now();

        [[nodiscard]] bool syncIsAdvancedWithNonRootFolder() const;
        [[nodiscard]] RemoteNodeId rootFolderRemoteId() const;

        /**
         * @brief Returns the remote ID of the folder whose changes are watched, i.e. the folder used by the `longpoll`,
         * `listing/full` and `listing/continue` requests.
         *
         * It is the root folder of the remote drive, except for an advanced synchronization whose target folder is not
         * the root folder of the remote drive, in which case it is the target folder of the synchronization.
         */
        [[nodiscard]] ExitInfo getWatchedFolderRemoteId(RemoteNodeId &watchedFolderRemoteId) const;

        [[nodiscard]] ExitInfo parseCsvReply(CursorPersistence cursorPersistence,
                                             std::shared_ptr<CsvFullFileListWithCursorJob> csvFullListingJob);
        [[nodiscard]] ExitInfo handleCsvReplyCursor(const RemoteNodeId &remoteDirId, CursorPersistence cursorPersistence,
                                                    std::shared_ptr<CsvFullFileListWithCursorJob> csvFullFileListWithCursorJob);
        [[nodiscard]] ExitInfo createGetItemsInRemoteDirJob(
                const RemoteNodeId &remoteDirId, std::shared_ptr<CsvFullFileListWithCursorJob> &csvFullFileListWithCursorJob);

        [[nodiscard]] ExitInfo createLongPollJob(const RemoteNodeId &remoteDirId, std::shared_ptr<LongPollJob> &longPollJob);

        [[nodiscard]] virtual ExitInfo updateLongPollJob(const RemoteNodeId &remoteDirId,
                                                         std::shared_ptr<LongPollJob> &longPollJob);
        [[nodiscard]] ExitInfo processEvents(const RemoteNodeId &remoteDirId, std::shared_ptr<LongPollJob> &longPollJob);

        [[nodiscard]] ExitInfo processListingContinueResponse(const RemoteNodeId &remoteDirId,
                                                              std::shared_ptr<ContinueFileListWithCursorJob> &job,
                                                              CursorData &cursorData, bool &hasMore);

        [[nodiscard]] ExitInfo runListingContinueJob(const RemoteNodeId &remoteDirId,
                                                     std::shared_ptr<ContinueFileListWithCursorJob> &job,
                                                     const CursorData &cursorData);

        [[nodiscard]] ExitInfo handleRemoteSnapshotValidity(bool &remoteSnapshotCanBeReused);
        [[nodiscard]] ExitInfo checkIfRemoteSnapshotCanBeReused();
        [[nodiscard]] ExitInfo mainProcessingTask(const RemoteNodeId &remoteDirId, CursorData &cursorData, bool &hasMore);

        void abortAndClearLongPollJob(std::shared_ptr<LongPollJob> &longPollJob);

        ExitInfo setBlackList(bool &blackListChanged);
        [[nodiscard]] ExitInfo checkIfSharedFolderIsEmpty(bool &empty);
        [[nodiscard]] ExitInfo updateSharedFolderItem();

        friend class TestRemoteFileSystemObserverWorker;
};

} // namespace KDC
