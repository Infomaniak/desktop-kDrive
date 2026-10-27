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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "app/cache/cachetypes.h"
#include "app/errors/inotifywatchlimit.h"
#include "app/errors/syncerrorkind.h"
#include "libcommon/utility/types.h"

#include <QObject>

namespace KDC {

class ServiceActionTracker;
class SettingsWindowController;
class SyncErrorService;
class SyncService;
struct ClassifiedSyncError;

/**
 * Runs the action of an errors-page row: the only place that knows what each button does.
 *
 * The action comes from the error catalog (`describeSyncError`), so it always matches the button the row shows. Local
 * items open in the file manager, never outside the synchronization root; web pages open in the browser; backend
 * actions go through the services, which own their pending state and report failures.
 */
class SyncErrorActionHandler final : public QObject {
        Q_OBJECT

    public:
        SyncErrorActionHandler(SyncService &syncService, SyncErrorService &syncErrorService,
                               SettingsWindowController &settingsWindowController, ServiceActionTracker &serviceActionTracker,
                               QObject *parent = nullptr);

        /** Runs the action of `classified`, an error of the synchronization described by `context`. */
        void trigger(const ClassifiedSyncError &classified, const SyncContext &context);
        /** Runs `action` on `classified`: the row button goes through `trigger`, the explanation dialog buttons call it
         * directly. */
        void run(SyncErrorAction action, const ClassifiedSyncError &classified, const SyncContext &context);
        /** Whether "Raise the limit" waits for the authentication or the script, for the synchronization `syncDbId`. */
        [[nodiscard]] bool isWatchLimitRaisePending(qint64 syncDbId) const;

    signals:
        // The user must log in again; handled like the logged-out blocking screen.
        void loginRequested();
        // The "Understand the problem" button: the errors page opens the explanation dialog of this error. Also emitted
        // when "Raise the limit" is not available or fails (`raiseFailure`): the dialog then gives the command to run.
        void explanationRequested(ErrorDbId errorDbId, WatchLimitRaiseFailure raiseFailure = WatchLimitRaiseFailure::None);
        void watchLimitRaisePendingChanged(qint64 syncDbId);

    private:
        void raiseWatchLimit(const ClassifiedSyncError &classified, const SyncContext &context);
        void finishWatchLimitRaise(ErrorDbId errorDbId, qint64 syncDbId, WatchLimitRaiseFailure raiseFailure);

        SyncService &_syncService;
        SyncErrorService &_syncErrorService;
        SettingsWindowController &_settingsWindowController;
        ServiceActionTracker &_serviceActionTracker;
};

} // namespace KDC
