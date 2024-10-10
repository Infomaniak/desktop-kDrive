/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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
#include "libcommonserver/utility/utility.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

FileSystemObserverWorker::FileSystemObserverWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name,
                                                   const std::string &shortName, ReplicaSide side) :
    ISyncWorker(syncPal, name, shortName), _syncDb(syncPal->_syncDb), _snapshot(syncPal->snapshot(side)) {}

FileSystemObserverWorker::~FileSystemObserverWorker() {}

void FileSystemObserverWorker::invalidateSnapshot() {
    if (_snapshot->isValid()) {
        _snapshot->init();
        //    *_interruptSync = true;     // TODO : check if it is possible to avoid restarting the full sync is those cases
        LOG_SYNCPAL_DEBUG(_logger, (_snapshot->side() == ReplicaSide::Local ? "Local" : "Remote") << " snapshot invalidated");
    }
}

void FileSystemObserverWorker::forceUpdate() {
    const std::lock_guard<std::mutex> lock(_mutex);
    _updating = true;
}

} // namespace KDC
