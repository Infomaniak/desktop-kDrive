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

#include "sync.h"

namespace KDC {

Sync::Sync() :
    _dbId(0), _driveDbId(0), _localPath(std::filesystem::path()), _targetPath(std::filesystem::path()), _targetNodeId(NodeId()),
    _paused(false), _supportVfs(false), _virtualFileMode(VirtualFileMode::Off), _notificationsDisabled(false),
    _dbPath(std::filesystem::path()), _hasFullyCompleted(false), _navigationPaneClsid(std::string()),
    _listingCursor(std::string()), _listingCursorTimestamp(0) {}

Sync::Sync(int dbId, int driveDbId, const std::filesystem::path &localPath, const std::filesystem::path &targetPath,
           const NodeId &targetNodeId, bool paused, bool supportVfs, VirtualFileMode virtualFileMode, bool notificationsDisabled,
           const std::filesystem::path &dbPath, bool hasFullyCompleted, const std::string &navigationPaneClsid,
           const std::string &listingCursor, int64_t listingCursorTimestamp) :
    _dbId(dbId), _driveDbId(driveDbId), _localPath(localPath), _targetPath(targetPath), _targetNodeId(targetNodeId),
    _paused(paused), _supportVfs(supportVfs), _virtualFileMode(virtualFileMode), _notificationsDisabled(notificationsDisabled),
    _dbPath(dbPath), _hasFullyCompleted(hasFullyCompleted), _navigationPaneClsid(navigationPaneClsid),
    _listingCursor(listingCursor), _listingCursorTimestamp(listingCursorTimestamp) {}

} // namespace KDC
