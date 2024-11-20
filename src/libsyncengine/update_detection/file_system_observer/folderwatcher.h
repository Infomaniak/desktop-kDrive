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

#pragma once

#include "libcommon/utility/types.h"
#include "libcommonserver/log/log.h"

#include <list>
#include <thread>

namespace KDC {

class LocalFileSystemObserverWorker;

class FolderWatcher {
    public:
        FolderWatcher(LocalFileSystemObserverWorker *parent, const SyncPath &rootFolder);
        virtual ~FolderWatcher() = default;

        const log4cplus::Logger &logger() const { return _logger; }

        void start();
        void stop();

        // The FolderWatcher can only become unreliable on Linux
        inline bool isReliable() const { return _isReliable; }
        ExitInfo exitInfo() const { return _exitInfo; }

    protected:
        // Implement this method in your subclass with the code you want your thread to run
        virtual void startWatching() = 0;
        virtual void stopWatching() = 0;
        void setExitInfo(ExitInfo exitInfo) { _exitInfo = exitInfo; }
        log4cplus::Logger _logger;
        LocalFileSystemObserverWorker *_parent;
        SyncPath _folder;
        bool _isReliable = true;
        bool _stop = false;

    private:
        static void executeFunc(void *thisWorker);

        std::unique_ptr<std::thread> _thread = nullptr;
        ExitInfo _exitInfo = ExitCode::Ok;
};

} // namespace KDC
