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

#include "app/services/commservice.h"
#include "app/services/serviceactiontracker.h"
#include "app/services/serviceeventbus.h"

#include <QObject>

#include <functional>
#include <vector>

namespace KDC {

/**
 * High-level facade for the actions on sync errors.
 *
 * Role:
 * - sends the error requests through CommService;
 * - keeps the error cache signal-driven: the server removes and adds errors back through push signals;
 * - reports transient failures through ServiceEventBus;
 * - registers the pending state in ServiceActionTracker.
 */
class SyncErrorService : public QObject {
        Q_OBJECT

    public:
        explicit SyncErrorService(CommService &commService, ServiceActionTracker &serviceActionTracker,
                                  ServiceEventBus &serviceEventBus, QObject *parent = nullptr);

        // Asks the server to detect the errors of a synchronization again. Ignored while a refresh of the same
        // synchronization is pending.
        Q_INVOKABLE void refreshErrors(qint64 syncDbId);
        Q_INVOKABLE [[nodiscard]] bool isRefreshPending(qint64 syncDbId) const;

        using ConflictInfoCallback = std::function<void(bool ok, const NodeConflictInfo &info)>;
        using ResolutionCallback = std::function<void(bool ok)>;
        // Reads the version of a conflicted item on one side. `relativePath` is the local copy of the conflict on the local
        // side, the original path on the remote side.
        void fetchConflictInfo(qint64 syncDbId, const SyncPath &relativePath, ReplicaSide side,
                               const ConflictInfoCallback &callback) const;
        // Keeps the local or the remote version of each conflict of a synchronization. On success, the server removes the
        // resolved errors through push signals, possibly before `callback` runs.
        void resolveConflicts(qint64 syncDbId, const std::vector<ErrorDbId> &keepLocal, const std::vector<ErrorDbId> &keepRemote,
                              const ResolutionCallback &callback);
        // Keeps the same version for every conflict in `errorDbIds`; with `KeepMostRecent`, the server compares the dates of
        // each conflict itself. Tracked as the same pending action as `resolveConflicts`.
        void resolveConflictsQuick(qint64 syncDbId, const std::vector<ErrorDbId> &errorDbIds, ConflictResolutionStrategy strategy,
                                   const ResolutionCallback &callback);
        [[nodiscard]] bool isConflictResolutionPending(qint64 syncDbId) const;

    signals:
        void refreshPendingChanged(qint64 syncDbId);
        void conflictResolutionPendingChanged(qint64 syncDbId);

    private:
        CommService &_commService;
        ServiceActionTracker &_serviceActionTracker;
        ServiceEventBus &_serviceEventBus;
};

} // namespace KDC
