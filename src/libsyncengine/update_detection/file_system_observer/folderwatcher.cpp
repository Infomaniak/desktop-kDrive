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

#include "folderwatcher.h"
#include "localfilesystemobserverworker.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

FolderWatcher::FolderWatcher(LocalFileSystemObserverWorker *parent, const SyncPath &path)
    : _logger(Log::instance()->getLogger()), _parent(parent), _folder(path) {}

FolderWatcher::~FolderWatcher() {
    if (_thread) {
        stop();
    }
}

void FolderWatcher::start() {
    LOG_DEBUG(_logger, "Start Folder Watcher");
    _stop = false;

    _thread = std::unique_ptr<std::jthread>(new std::jthread(executeFunc, this));

#if defined(__APPLE__)
    _thread->detach();
#endif
}

void FolderWatcher::stop() {
    LOG_DEBUG(_logger, "Stop Folder Watcher");
    _stop = true;

    stopWatching();

#if !defined(__APPLE__)
    if (_thread && _thread->joinable()) {
        _thread->join();
        _thread.release();
    }
#endif

    _thread = nullptr;
}

void *FolderWatcher::executeFunc(void *thisWorker) {
    ((FolderWatcher *)thisWorker)->startWatching();
    return nullptr;
}

}  // namespace KDC
