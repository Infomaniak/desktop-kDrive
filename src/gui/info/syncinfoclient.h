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

        inline void setPaused(bool paused) { _paused = paused; }
        inline bool paused() const { return _paused; }
        inline void setStatus(SyncStatus status) { _status = status; }
        inline SyncStatus status() const { return _status; }
        inline void setStep(SyncStep step) { _step = step; }
        inline SyncStep step() const { return _step; }
        inline void setUnresolvedConflicts(bool unresolvedConflicts) { _unresolvedConflicts = unresolvedConflicts; }
        inline bool unresolvedConflicts() const { return _unresolvedConflicts; }
        inline void setCurrentFile(qint64 currentFile) { _currentFile = currentFile; }
        inline qint64 currentFile() const { return _currentFile; }
        inline void setTotalFiles(qint64 totalFiles) { _totalFiles = totalFiles; }
        inline qint64 totalFiles() const { return _totalFiles; }
        inline void setCompletedSize(qint64 completedSize) { _completedSize = completedSize; }
        inline qint64 completedSize() const { return _completedSize; }
        inline void setTotalSize(qint64 totalSize) { _totalSize = totalSize; }
        inline qint64 totalSize() const { return _totalSize; }
        inline void setEstimatedRemainingTime(qint64 estimatedRemainingTime) { _estimatedRemainingTime = estimatedRemainingTime; }
        inline qint64 estimatedRemainingTime() const { return _estimatedRemainingTime; }

        QString name() const;

    private:
        bool _paused;
        SyncStatus _status;
        SyncStep _step;
        bool _unresolvedConflicts;
        qint64 _currentFile;
        qint64 _totalFiles;
        qint64 _completedSize;
        qint64 _totalSize;
        qint64 _estimatedRemainingTime;
};

}  // namespace KDC
