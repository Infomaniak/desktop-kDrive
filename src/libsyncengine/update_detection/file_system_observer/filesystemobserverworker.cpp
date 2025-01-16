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

#include "filesystemobserverworker.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

constexpr int maxRetryBeforeInvalidation = 3;

FileSystemObserverWorker::FileSystemObserverWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name,
                                                   const std::string &shortName, const ReplicaSide side) :
    ISyncWorker(syncPal, name, shortName), _syncDb(syncPal->_syncDb), _snapshot(syncPal->snapshot(side)) {}

void FileSystemObserverWorker::invalidateSnapshot() {
    if (!_snapshot->isValid()) return;
    // The synchronisation will restart, even if there is no change in the file system and if the snapshot is not actually invalidated.
    _syncPal->setRestart(true);

    _invalidateCounter++;
    if (_invalidateCounter < maxRetryBeforeInvalidation) {
        LOG_SYNCPAL_DEBUG(_logger, _snapshot->side()
                                           << " snapshot is not invalidated. Invalidation count: " << _invalidateCounter);
        return;
    }

    _snapshot->init();
    _invalidateCounter = 0;
    LOG_SYNCPAL_DEBUG(_logger, _snapshot->side() << " snapshot invalidated");
}

void FileSystemObserverWorker::forceUpdate() {
    const std::scoped_lock lock(_mutex);
    _updating = true;
}

} // namespace KDC
