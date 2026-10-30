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
#include "libcommon/data/user.h"
#include "libsyncengine/jobs/local/openfilejob.h"

#include <QObject>

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
  and offer to cancel the request. The `finished`
  signal is emitted once the request has been processed; the caller is expected to destroy the handler afterwards
  (e.g. by connecting the signal to QObject::deleteLater).
*/
class FileOpeningHandler : public QObject {
        Q_OBJECT

    public:
        struct Request {
                User user;
                DriveId driveId{0};
                RemoteNodeId remoteNodeId;
        };

        FileOpeningHandler(AppServer *appServer, std::string url, QObject *parent = nullptr);

        //! Parses the URL given to the constructor and queues an OpenFileJob performing the file opening flow.
        void queueOpenFileJob();

        //! Id of the queued OpenFileJob. Only meaningful once queueOpenFileJob has been called.
        [[nodiscard]] UniqueId jobId() const { return _jobId; }

        //! Aborts the queued OpenFileJob and cancels the hydration of the item, if it is in progress. Invoked when the
        //! user cancels the request from the client interface.
        [[nodiscard]] ExitInfo cancel();

        //! Returns true if `urlStr` is a file opening URL, i.e. "kdrive://open/...".
        static bool isFileOpeningDeeplink(const std::string &urlStr);
        //! Extracts the user id, the drive id, and the node id from `urlStr`. Returns false if the URL is not a valid
        //! file opening URL.
        static bool parseDeeplinkUrl(const std::string &urlStr, Request &request);
        //! Returns true if `path` designates executable content that must be revealed in its parent folder instead of
        //! being launched.
        static bool shouldOpenParentFolder(const SyncPath &path);

    signals:
        //! Emitted once the file opening request has been processed, with true on success and false on failure. The
        //! caller is expected to destroy the handler afterwards (e.g. by connecting this signal to
        //! QObject::deleteLater).
        void finished(bool success);

    private:
        //! Invoked on the application thread when the OpenFileJob has completed: opens the item on success, and
        //! notifies the user of the failure otherwise.
        void onOpenFileJobFinished(OpenFileJob::Outcome outcome, const SyncPath &localPath, SyncDbId syncDbId);
        [[nodiscard]] ExitInfo openLocalItem(const SyncPath &localPath, SyncDbId syncDbId);
        [[nodiscard]] bool isSyncEngineRunning(SyncDbId syncDbId) const;
        void notifyUser(const std::string &message) const;
        void handleFileOpeningSuccess();
        void handleFileOpeningFailure(const std::string &userMessage);
        void handleFileOpeningCanceled();
        //! Invoked on the application thread when the OpenFileJob has reported a new state: forwards it, along with the
        //! name of the item being opened, to the client interface.
        void onFileOpeningStatus(FileOpeningPhase phase, int8_t percent, const CommString &itemName) const;

        AppServer *_appServer{nullptr};
        std::string _url;
        UniqueId _jobId{0};
        std::weak_ptr<OpenFileJob> _job;
};

} // namespace KDC
