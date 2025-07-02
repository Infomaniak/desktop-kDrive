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

#include "libparms/parmslib.h"
#include "libcommon/utility/types.h"

#include <string>
#include <filesystem>

namespace KDC {

class PARMS_EXPORT Sync {
    public:
        Sync();
        Sync(int dbId, int driveDbId, const SyncPath &localPath, const NodeId &localNodeId, const SyncPath &targetPath,
             const NodeId &targetNodeId = NodeId(), bool paused = false, bool supportVfs = false,
             VirtualFileMode virtualFileMode = VirtualFileMode::Off, bool notificationsDisabled = false,
             const SyncPath &dbPath = SyncPath(), bool hasFullyCompleted = false,
             const std::string &navigationPaneClsid = std::string(), const std::string &listingCursor = std::string(),
             int64_t listingCursorTimestamp = 0);

        inline void setDbId(int dbId) { _dbId = dbId; }
        inline int dbId() const { return _dbId; }
        inline void setDriveDbId(int driveDbId) { _driveDbId = driveDbId; }
        inline int driveDbId() const { return _driveDbId; }
        inline void setLocalPath(const SyncPath &localPath) { _localPath = localPath; }
        inline const SyncPath &localPath() const { return _localPath; }
        inline void setLocalNodeId(const NodeId &localNodeId) { _localNodeId = localNodeId; }
        inline const NodeId &localNodeId() const { return _localNodeId; }
        inline void setTargetPath(const SyncPath &targetPath) { _targetPath = targetPath; }
        inline const SyncPath &targetPath() const { return _targetPath; }
        inline void setTargetNodeId(const NodeId &targetNodeId) { _targetNodeId = targetNodeId; }
        inline const NodeId &targetNodeId() const { return _targetNodeId; }
        inline void setDbPath(const SyncPath &dbPath) { _dbPath = dbPath; }
        inline const SyncPath &dbPath() const { return _dbPath; }
        inline void setPaused(bool paused) { _paused = paused; }
        inline bool paused() const { return _paused; }
        inline void setSupportVfs(bool supportVfs) { _supportVfs = supportVfs; }
        inline bool supportVfs() const { return _supportVfs; }
        inline void setVirtualFileMode(VirtualFileMode virtualFileMode) { _virtualFileMode = virtualFileMode; }
        inline VirtualFileMode virtualFileMode() const { return _virtualFileMode; }
        inline void setNotificationsDisabled(bool notificationsDisabled) { _notificationsDisabled = notificationsDisabled; }
        inline bool notificationsDisabled() const { return _notificationsDisabled; }
        inline void setHasFullyCompleted(bool hasFullyCompleted) { _hasFullyCompleted = hasFullyCompleted; }
        inline bool hasFullyCompleted() const { return _hasFullyCompleted; }
        inline void setNavigationPaneClsid(const std::string &navigationPaneClsid) { _navigationPaneClsid = navigationPaneClsid; }
        inline const std::string &navigationPaneClsid() const { return _navigationPaneClsid; }
        inline void setListingCursor(const std::string &listingCursor, int64_t timestamp) {
            _listingCursor = listingCursor;
            _listingCursorTimestamp = timestamp;
        }
        inline void listingCursor(std::string &listingCursor, int64_t &timestamp) const {
            listingCursor = _listingCursor;
            timestamp = _listingCursorTimestamp;
        }

    private:
        int _dbId{0};
        int _driveDbId{0};
        SyncPath _localPath;
        NodeId _localNodeId;
        SyncPath _targetPath;
        NodeId _targetNodeId;
        bool _paused{false};
        bool _supportVfs{false};
        VirtualFileMode _virtualFileMode{VirtualFileMode::Off};
        bool _notificationsDisabled{false};
        SyncPath _dbPath;
        bool _hasFullyCompleted{false};
        std::string _navigationPaneClsid;
        std::string _listingCursor;
        int64_t _listingCursorTimestamp{0};
};

} // namespace KDC
