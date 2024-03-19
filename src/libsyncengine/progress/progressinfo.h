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

#include "syncfileitem.h"
#include "estimates.h"
#include "progress.h"
#include "progressitem.h"

#include <cstdint>
#include <map>
#include <queue>
#include <list>

namespace KDC {

class SyncPal;

class ProgressInfo {
    public:
        ProgressInfo(std::shared_ptr<SyncPal> syncPal);
        ~ProgressInfo();

        void reset();
        inline void setUpdate(bool value) { _update = value; }
        inline bool update() const { return _update; }
        void updateEstimates();
        void initProgress(const SyncFileItem &item);
        void setProgress(const SyncPath &path, int64_t completed);
        void setProgressComplete(const SyncPath &path, SyncFileStatus status);
        bool getSyncFileItem(const SyncPath &path, SyncFileItem &item);

        inline int64_t totalFiles() const { return _fileProgress.total(); }
        inline int64_t completedFiles() const { return _fileProgress.completed(); }
        inline int64_t totalSize() const { return _sizeProgress.total(); }
        inline int64_t completedSize() const { return _sizeProgress.completed(); }
        inline int64_t currentFile() const { return completedFiles(); }
        Estimates totalProgress() const;

    private:
        std::shared_ptr<SyncPal> _syncPal;
        std::map<SyncPath, std::queue<ProgressItem>>
            _currentItems;  // Use a queue here because in a few cases, we can have several operations on the same path (e.g.:
                            // DELETE a file and CREATE a directory with exact same name)
        Progress _sizeProgress;
        Progress _fileProgress;
        int64_t _totalSizeOfCompletedJobs;
        double _maxFilesPerSecond;
        double _maxBytesPerSecond;
        bool _update;

        int64_t optimisticEta() const;
        bool trustEta() const;
        Estimates fileProgress(const SyncFileItem &item);
        void recomputeCompletedSize();
        bool isSizeDependent(const SyncFileItem &item) const;
};

}  // namespace KDC
