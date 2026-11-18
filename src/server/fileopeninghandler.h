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

#include "libcommon/utility/types.h"
#include "libsyncengine/jobs/local/openfilejob.h"

#include <QString>

#include <memory>
#include <string>

namespace KDC {

class AppServer;

//! Handles "kdrive://open/user/<user id>/drive/<drive id>/file/<node id>" URLs (e.g. clicked in a web browser).
/*!
  The user id and the drive id are the server-side kDrive user id and drive id, as displayed in the web application
  URLs. The node id is the server-side id of the file or folder to open.

  The handler queues an OpenFileJob which:
  - resolves the node id against the syncs configured for the drive, waiting for its synchronization if needed,
  - makes the item available locally, hydrating it first if it is a dehydrated LiteSync placeholder.

  When the job completes, the handler opens the item on success, and notifies the user otherwise. While the job runs,
  the handler forwards its state to the client interface so that a pop-up window can inform the user of the progress
  and offer to cancel the request. Once the request has been processed, the handler unregisters itself from the
  application server, which releases its last owning reference and destroys it on the application thread.

  The handler must be created through std::make_shared: the registry of the application server owns it through a
  shared pointer, while the callbacks of the OpenFileJob only hold weak references and are discarded if the handler
  has already been destroyed.
*/
class FileOpeningHandler : public std::enable_shared_from_this<FileOpeningHandler> {
    public:
        FileOpeningHandler(AppServer *const appServer, std::string url);

        //! Parses the URL given to the constructor and queues an OpenFileJob performing the file opening flow.
        void queueOpenFileJob();

        //! Id of the queued OpenFileJob. Only meaningful once queueOpenFileJob has been called.
        [[nodiscard]] UniqueId jobId() const { return _jobId; }

        //! Aborts the queued OpenFileJob and cancels the hydration of the item, if it is in progress. Invoked when the
        //! user cancels the request from the client interface.
        [[nodiscard]] ExitInfo cancel();

        //! Returns true if `urlStr` is a file opening URL, i.e. "kdrive://open/...".
        static bool isFileOpeningDeeplink(const std::string &urlStr);
        //! Returns true if `path` designates executable content that must be revealed in its parent folder instead of
        //! being launched.
        static bool shouldOpenParentFolder(const SyncPath &path);
        //! Returns the maximum number of file-opening flows that may run concurrently. A running flow holds one
        //! job-pool worker while it waits (for the item to be synchronized, then for its download) and needs one more
        //! worker for the download itself. At most half of the usable workers, i.e. the pool capacity minus the
        //! workers JobManager reserves for PRIO_HIGHEST jobs, may be pinned by waiting flows, so that regular sync
        //! jobs keep running.
        [[nodiscard]] static Count maxConcurrentFileOpeningJobs(Count jobPoolCapacity, Count reservedWorkerCount);
        //! Same as above, computed from the current job-pool state of the sync job manager.
        [[nodiscard]] static Count maxConcurrentFileOpeningJobs();

    private:
        //! Invoked on the application thread when the OpenFileJob has completed: opens the item on success, and
        //! notifies the user of the failure otherwise.
        void onOpenFileJobFinished(OpenFileJob::Outcome outcome, const SyncPath &localPath, SyncDbId syncDbId);
        [[nodiscard]] ExitInfo openLocalItem(const SyncPath &localPath, SyncDbId syncDbId);
        [[nodiscard]] bool isSyncEngineRunning(SyncDbId syncDbId) const;
        void handleFileOpeningSuccess();
        //! Reports the failure of the request to the client interface, which shows its failure window with it. The
        //! window offers to download the item from the kDrive web application, at the address returned by webUrl().
        void handleFileOpeningFailure();
        //! Returns the kDrive web application address of the requested item, empty when the request URL is invalid.
        [[nodiscard]] CommString webUrl() const;
        void handleFileOpeningCanceled();
        //! Invoked on the application thread when the OpenFileJob has reported a new state: forwards it, along with
        //! the name of the item being opened, the path of its parent folder, and the download progress in bytes, to
        //! the client interface.
        void onFileOpeningStatus(FileOpeningPhase phase, int8_t percent, const CommString &itemName, const CommString &itemPath,
                                 int64_t receivedBytes, int64_t totalBytes) const;

        AppServer *_appServer{nullptr};
        std::string _url;
        UniqueId _jobId{0};
        std::weak_ptr<OpenFileJob> _job;

        friend class AppServer;
};

} // namespace KDC
