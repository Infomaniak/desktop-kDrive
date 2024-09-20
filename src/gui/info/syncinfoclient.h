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

#include "libcommon/info/syncinfo.h"

namespace KDC {

class SyncInfoClient : public SyncInfo {
    public:
        SyncInfoClient();
        SyncInfoClient(const SyncInfo &syncInfo);

        inline void setPaused(bool paused) noexcept { _paused = paused; }
        inline bool paused() const noexcept { return _paused; }
        inline void setStatus(SyncStatus status) noexcept { _status = status; }
        inline SyncStatus status() const noexcept { return _status; }
        inline void setStep(SyncStep step) noexcept { _step = step; }
        inline SyncStep step() const noexcept { return _step; }
        inline void setUnresolvedConflicts(bool unresolvedConflicts) noexcept { _unresolvedConflicts = unresolvedConflicts; }
        inline bool unresolvedConflicts() const noexcept { return _unresolvedConflicts; }
        inline void setCurrentFile(qint64 currentFile) noexcept { _currentFile = currentFile; }
        inline qint64 currentFile() const noexcept { return _currentFile; }
        inline void setTotalFiles(qint64 totalFiles) { _totalFiles = totalFiles; }
        inline qint64 totalFiles() const noexcept { return _totalFiles; }
        inline void setCompletedSize(qint64 completedSize) noexcept { _completedSize = completedSize; }
        inline qint64 completedSize() const noexcept { return _completedSize; }
        inline void setTotalSize(qint64 totalSize) noexcept { _totalSize = totalSize; }
        inline qint64 totalSize() const noexcept { return _totalSize; }
        inline void setEstimatedRemainingTime(qint64 estimatedRemainingTime) noexcept {
            _estimatedRemainingTime = estimatedRemainingTime;
        }
        inline qint64 estimatedRemainingTime() const noexcept { return _estimatedRemainingTime; }
        inline void setIsBeingDeleted(bool isDeletionOnGoing) noexcept { _isBeingDeleted = isDeletionOnGoing; }
        inline bool isBeingDeleted() const noexcept { return _isBeingDeleted; }

        QString name() const;

    private:
        bool _paused{false};
        SyncStatus _status{SyncStatus::Undefined};
        SyncStep _step;
        bool _unresolvedConflicts{false};
        qint64 _currentFile{0};
        qint64 _totalFiles{0};
        qint64 _completedSize{0};
        qint64 _totalSize{0};
        qint64 _estimatedRemainingTime{0};
        bool _isBeingDeleted{false};
};

} // namespace KDC
