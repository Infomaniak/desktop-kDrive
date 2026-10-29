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

#include "jobs/syncjob.h"
#include "libcommon/data/user.h"
#include "syncpal/syncpal.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace KDC {

//! Asynchronous job performing the whole "open the item indicated by a kDrive deeplink" flow.
/*!
 * The job first resolves the item indicated by the URL against the syncs configured for the drive. While the item has
 * not been synchronized yet, the job checks every 2 seconds whether it still exists on the remote side, and keeps
 * resolving it during 10 minutes before giving up with a timeout error.
 *
 * Once the item has been resolved, it is made available locally:
 * - if it is not a dehydrated placeholder, it can be opened right away;
 * - otherwise, the job enqueues a HydratePlaceHolderJob and waits for the download to complete, during 10 hours at
 *   most, before giving up with a timeout error.
 *
 * The job must be provided with two callbacks giving access to the state of the running syncs, as this state is owned
 * by the application layer:
 * - `getSyncPal` returns the SyncPal of a sync whose engine is running, nullptr when the engine is not running;
 * - `getVfs` returns the VFS instance of a sync, nullptr when the sync has no VFS.
 *
 * The last step of the flow, i.e. actually opening the item and providing feedback to the user, is left to the caller
 * as it requires GUI capabilities that a job must not have: when the job completes, inspect outcome() and localPath().
 */
class OpenFileJob : public SyncJob {
    public:
        //! Returns the SyncPal of a sync whose engine is running, nullptr when the engine is not running.
        using GetSyncPalCallback = std::function<std::shared_ptr<SyncPal>(SyncDbId)>;
        //! Returns the VFS instance of a sync, nullptr when the sync has no VFS.
        using GetVfsCallback = std::function<std::shared_ptr<Vfs>(SyncDbId)>;

        //! Outcome of the job, allowing the caller to provide the appropriate user feedback.
        enum class Outcome {
            Failed, //!< An error occurred: the item could not be opened (see exitInfo() for details).
            NotFound, //!< The item does not exist on the remote side anymore.
            ResolveTimedOut, //!< The item was not synchronized before the deadline.
            HydrationTimedOut, //!< The hydration did not complete before the deadline.
            ReadyToOpen, //!< The item is available locally: the caller can open it.
        };

        struct Request {
                User user;
                DriveId driveId{0};
                RemoteNodeId remoteNodeId;
        };

        //! \param url is a "kdrive://open/user/<user id>/drive/<drive id>/file/<node id>" URL.
        OpenFileJob(std::string url, GetSyncPalCallback getSyncPal, GetVfsCallback getVfs);

        //! Returns true if `urlStr` is a file opening URL, i.e. "kdrive://open/...", and extracts the user id, the
        //! drive id, and the node id from it.
        static bool parseDeeplinkUrl(const std::string &urlStr, Request &request);

        [[nodiscard]] Outcome outcome() const { return _outcome; }
        //! Absolute path of the resolved item. Only meaningful when outcome() is Outcome::ReadyToOpen.
        [[nodiscard]] const SyncPath &localPath() const { return _localPath; }
        //! Id of the sync through which the item has been resolved. Only meaningful when outcome() is
        //! Outcome::ReadyToOpen.
        [[nodiscard]] SyncDbId syncDbId() const { return _syncDbId; }

    protected:
        ExitInfo canRun() override;

    private:
        ExitInfo runJob() override;

        //! Resolves the item indicated by the URL, retrying every 2 seconds while it is being synchronized.
        ExitInfo resolveItem();
        //! Tries to resolve the remote node id into one of the syncs of the drive. Returns an error on DB or API access
        //! error, and sets `found` with false if no admissible sync has been found.
        ExitInfo resolveDeeplink(bool &found);
        //! Sorts `syncs` by examining the syncs whose engine is running first, and makes the examination order
        //! deterministic.
        void sortSyncsByRunningState(std::vector<Sync> &syncs) const;
        //! Returns the live SyncDb of a sync whose engine is running, nullptr otherwise.
        std::shared_ptr<SyncDb> getSyncDb(SyncDbId syncDbId) const;
        //! Looks for the best admissible sync containing the requested item. Sets `found` with false if none has been
        //! found.
        ExitInfo getBestAdmissibleSync(const std::vector<Sync> &syncs, Sync &bestSync, SyncPath &relativePath, bool &found);
        //! Checks whether the item exists on the remote side.
        ExitInfo checkRemoteItemExistence(bool &exists) const;
        //! Opens the resolved item if it is available locally, or hydrates it first if it is a dehydrated placeholder.
        ExitInfo openResolvedLocalItem();
        //! Enqueues the hydration of the dehydrated placeholder and waits for the download to complete.
        ExitInfo hydrateResolvedPlaceholder();
        //! Marks the job as ready to open the item and returns `ExitCode::Ok`.
        ExitInfo readyToOpen();

        const std::string _url;
        const GetSyncPalCallback _getSyncPal;
        const GetVfsCallback _getVfs;

        Request _request;
        std::unordered_set<DriveDbId> _driveDbIds;
        DriveDbId _driveDbId{0};
        SyncDbId _syncDbId{0};
        SyncPath _relativePath; // Relative to the local folder of the sync.
        SyncPath _localPath; // Absolute path of the item on the local file system.

        // Read-only instances of the SyncDb of the syncs whose engine is not running, cached to avoid reopening them
        // on every resolution attempt.
        std::unordered_map<SyncDbId, std::shared_ptr<SyncDb>> _tempSyncDbMap;

        Outcome _outcome{Outcome::Failed};
};

} // namespace KDC
