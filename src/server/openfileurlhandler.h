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

#include <QTimer>
#include <QUrl>

#include <memory>
#include <unordered_map>

namespace KDC {

class AppServer;
class SyncDb;

//! Handles "kdrive://open/drive/<drive id>/file/<node id>" URLs (e.g. clicked in a web browser).
/*!
  The drive id is the server-side drive id, as displayed in the web application URLs. The node id is the
  server-side id of the file or folder to open.

  The handler resolves the node id against the syncs configured for the drive, then:
  - opens the item directly if it is available locally,
  - triggers its hydration first if it is a dehydrated LiteSync placeholder,
  - waits for its synchronization if it exists on the remote side only.
*/
class OpenFileUrlHandler : public QObject {
        Q_OBJECT

    public:
        struct Request {
                DriveId driveId{0};
                NodeId nodeId;
        };

        OpenFileUrlHandler(AppServer *appServer, const QUrl &url, QObject *parent = nullptr);

        void start();

        //! Returns true if `url` is a file opening URL, i.e. "kdrive://open/...".
        static bool isOpenFileUrl(const QUrl &url);
        //! Extracts the drive id and the node id from `url`. Returns false if the URL is not a valid file opening URL.
        static bool parseUrl(const QUrl &url, Request &request);
        //! Returns true if `path` designates executable content that must be revealed in its parent folder instead of
        //! being launched.
        static bool shouldOpenParentFolder(const SyncPath &path);

    signals:
        void finished(bool success);

    private:
        enum class Phase {
            Resolve, // Look for the node in the syncs configured for the drive.
            CheckRemoteExistence, // The node is not synchronized yet, check whether it exists on the remote side.
            WaitForSync, // The node exists on the remote side only, wait for it to be synchronized.
            WaitForHydration, // The node is being hydrated, wait for the download to complete.
            Done
        };

        void processResolvePhase();
        //! Tries to resolve the node id into one of the syncs of the drive. Returns false on DB access error, and sets
        //! `found` with true if the node has been resolved.
        bool resolveNode(bool &found);
        void checkRemoteExistence();
        void onRemoteExistenceChecked(const ExitInfo &exitInfo, bool exists);
        void onSyncWaitTick();
        void onDeadlineReached();
        void openOrHydrate();
        bool triggerHydration();
        void onHydrationFinished(bool success);
        void openResolvedItem();
        void setPhase(Phase phase, std::chrono::milliseconds tickInterval, std::chrono::milliseconds timeout);
        void notifyUser(const QString &message) const;
        void succeed();
        void fail(const QString &userMessage);

        AppServer *_appServer{nullptr};
        QUrl _url;
        Request _request;
        Phase _phase{Phase::Resolve};

        QTimer _syncWaitTimer; // Periodically checks whether the item has been synchronized.
        QTimer _deadlineTimer; // Fails the request when the current phase takes too long.

        DriveDbId _driveDbId{0};
        SyncDbId _syncDbId{0};
        SyncPath _relativePath; // Relative to the local folder of the sync.
        SyncPath _localPath; // Absolute path of the item on the local file system.
        bool _waitingNotificationSent{false};

        // Read-only instances of the SyncDb of the syncs whose engine is not running, cached to avoid reopening them
        // on every resolution attempt.
        std::unordered_map<SyncDbId, std::shared_ptr<SyncDb>> _tempSyncDbMap;

#if defined(KD_MACOS)
        PinState _initialPinState{PinState::Unknown};
#endif
};

} // namespace KDC
