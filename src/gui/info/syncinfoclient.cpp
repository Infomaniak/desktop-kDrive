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
#include "syncinfoclient.h"
#include "libcommon/utility/utility.h"

#include <QDir>

namespace KDC {

SyncInfoClient::SyncInfoClient()
    : SyncInfo(),
      _paused(false),
      _status(SyncStatus::SyncStatusUndefined),
      _unresolvedConflicts(false),
      _currentFile(0),
      _totalFiles(0),
      _completedSize(0),
      _totalSize(0),
      _estimatedRemainingTime(0),
      _isBeingDeleted(false) {}

SyncInfoClient::SyncInfoClient(const SyncInfo &syncInfo)
    : SyncInfo(syncInfo),
      _paused(false),
      _status(SyncStatus::SyncStatusUndefined),
      _unresolvedConflicts(false),
      _currentFile(0),
      _totalFiles(0),
      _completedSize(0),
      _totalSize(0),
      _estimatedRemainingTime(0),
      _isBeingDeleted(false) {}

QString SyncInfoClient::name() const {
    return CommonUtility::getRelativePathFromHome(_localPath);
}

}  // namespace KDC
