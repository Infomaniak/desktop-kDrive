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
        explicit ProgressInfo(std::shared_ptr<SyncPal> syncPal);
        ~ProgressInfo();

        void reset();
        void setUpdate(bool value) { _update = value; }
        [[nodiscard]] bool update() const { return _update; }
        void updateEstimates();
        [[nodiscard]] bool initProgress(const SyncFileItem &item);
        [[nodiscard]] bool setProgress(const SyncPath &path, int64_t completed);
        [[nodiscard]] bool setProgressComplete(const SyncPath &path, SyncFileStatus status);
        [[nodiscard]] bool setSyncFileItemRemoteId(const SyncPath &path, const NodeId& remoteId);
        [[nodiscard]] bool getSyncFileItem(const SyncPath &path, SyncFileItem &item);

        [[nodiscard]] int64_t totalFiles() const { return _fileProgress.total(); }
        [[nodiscard]] int64_t completedFiles() const { return _fileProgress.completed(); }
        [[nodiscard]] int64_t totalSize() const { return _sizeProgress.total(); }
        [[nodiscard]] int64_t completedSize() const { return _sizeProgress.completed(); }
        [[nodiscard]] int64_t currentFile() const { return completedFiles(); }
        [[nodiscard]] Estimates totalProgress() const;

    private:
        std::shared_ptr<SyncPal> _syncPal;
        std::map<SyncPath, std::queue<ProgressItem>>
                _currentItems; // Use a queue here because in a few cases, we can have several operations on the same path (e.g.:
                               // DELETE a file and CREATE a directory with exact same name)
        Progress _sizeProgress;
        Progress _fileProgress;
        int64_t _totalSizeOfCompletedJobs{0};
        double _maxFilesPerSecond{0.0};
        double _maxBytesPerSecond{0.0};
        bool _update{false};

        [[nodiscard]] int64_t optimisticEta() const;
        [[nodiscard]] bool trustEta() const;
        void recomputeCompletedSize();
        [[nodiscard]] bool isSizeDependent(const SyncFileItem &item) const;
};

} // namespace KDC
