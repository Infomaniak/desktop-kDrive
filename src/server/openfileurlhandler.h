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

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QUrl>

namespace KDC {

class AppServer;
class Sync;

//! Handles "kdrive://open/<file path>[?driveId=<drive id>]" URLs (e.g. clicked in a web browser).
/*!
  The file path is relative to the local folder of a sync. The optional `driveId` query item restricts the
  search to the syncs of a drive (`driveId` is the server side drive id, as displayed in web application URLs).

  The handler resolves the file path against the configured syncs, then:
  - opens the item directly if it is available locally,
  - triggers its hydration first if it is a dehydrated LiteSync placeholder,
  - waits for its synchronization if it exists on the remote side only.
*/
class OpenFileUrlHandler : public QObject {
        Q_OBJECT

    public:
        struct Request {
                SyncPath relativePath;
                DriveId driveId{0};
                bool hasDriveId{false};
        };

        OpenFileUrlHandler(AppServer *appServer, const QUrl &url, QObject *parent = nullptr);

        void start();

        //! Returns true if `url` is a file opening URL, i.e. "kdrive://open/...".
        static bool isOpenFileUrl(const QUrl &url);
        //! Extracts the file path and the optional drive id from `url`. Returns false if the URL is invalid or unsafe.
        static bool parseUrl(const QUrl &url, Request &request);
        //! Returns true if `relativePath` is not empty, relative and does not contain any "." or ".." component.
        static bool isRelativePathSafe(const SyncPath &relativePath);
        //! Returns true if `path` designates executable content that must be revealed in its parent folder instead of
        //! being launched.
        static bool shouldOpenParentFolder(const SyncPath &path);

    signals:
        void finished(bool success);

    private:
        enum class Phase {
            Resolve, // Look for the file in the configured syncs.
            WaitForSync, // The file exists on the remote side only, wait for it to appear locally.
            WaitForHydration // The file is being hydrated, wait for the download to complete.
        };

        void onTick();
        void processResolvePhase();
        void processWaitForSyncPhase();
        void processWaitForHydrationPhase();
        void hydrateOrOpen();
        bool triggerHydration();
        void openResolvedItem();
        bool candidateSyncs(std::vector<Sync> &syncs);
        void setPhase(Phase phase, std::chrono::milliseconds tickInterval);
        void notifyUser(const QString &message) const;
        void succeed();
        void fail(const QString &userMessage);

        AppServer *_appServer{nullptr};
        QUrl _url;
        Request _request;
        Phase _phase{Phase::Resolve};
        QTimer _tickTimer;
        QElapsedTimer _phaseTimer;
        SyncDbId _syncDbId{0};
        SyncPath _localPath;
        bool _waitingNotificationSent{false};
        int _downloadNotOngoingCount{0};
#if defined(KD_MACOS)
        PinState _initialPinState{PinState::Unknown};
#endif
};

} // namespace KDC
