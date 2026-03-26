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
        Sync(SyncDbId dbId, DriveDbId driveDbId, const SyncPath &localPath, const NodeId &localNodeId, const SyncPath &targetPath,
             const NodeId &targetNodeId = NodeId(), bool paused = false, bool supportVfs = false,
             VirtualFileMode virtualFileMode = VirtualFileMode::Off, bool notificationsDisabled = false,
             const SyncPath &dbPath = SyncPath(), bool hasFullyCompleted = false,
             const std::string &navigationPaneClsid = std::string(), CursorStore cursorStore = {});

        inline void setDbId(SyncDbId dbId) { _dbId = dbId; }
        inline SyncDbId dbId() const { return _dbId; }
        inline void setDriveDbId(const DriveDbId driveDbId) { _driveDbId = driveDbId; }
        inline DriveDbId driveDbId() const { return _driveDbId; }
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
        inline void setSupportVfs(const bool supportVfs) { _supportVfs = supportVfs; }
        inline bool supportVfs() const { return _supportVfs; }
        inline void setVirtualFileMode(const VirtualFileMode virtualFileMode) { _virtualFileMode = virtualFileMode; }
        inline VirtualFileMode virtualFileMode() const { return _virtualFileMode; }
        inline void setNotificationsDisabled(const bool notificationsDisabled) { _notificationsDisabled = notificationsDisabled; }
        inline bool notificationsDisabled() const { return _notificationsDisabled; }
        inline void setHasFullyCompleted(bool hasFullyCompleted) { _hasFullyCompleted = hasFullyCompleted; }
        inline bool hasFullyCompleted() const { return _hasFullyCompleted; }
        inline void setNavigationPaneClsid(const std::string &navigationPaneClsid) { _navigationPaneClsid = navigationPaneClsid; }
        inline const std::string &navigationPaneClsid() const { return _navigationPaneClsid; }

        CursorStore getCursorStore() const;
        void setCursorStore(const CursorStore &cursors);

        inline void setUserPrivateFolderCursor(const Cursor &listingCursor, Timestamp timestamp) {
            _cursorStore.userPrivateFolderCursor = {listingCursor, timestamp};
        }
        inline void setCommonDocumentsFolderCursor(const Cursor &listingCursor, Timestamp timestamp) {
            _cursorStore.commonDocumentsFolderCursor = {listingCursor, timestamp};
        }
        inline void setSharedFolderCursor(const Cursor &listingCursor, Timestamp timestamp) {
            _cursorStore.sharedFolderCursor = {listingCursor, timestamp};
        }

        inline void userPrivateFolderCursor(Cursor &listingCursor, Timestamp &timestamp) const {
            listingCursor = _cursorStore.userPrivateFolderCursor.cursor;
            timestamp = _cursorStore.userPrivateFolderCursor.timestamp;
        }
        inline void commonDocumentsFolderCursor(Cursor &listingCursor, Timestamp &timestamp) const {
            listingCursor = _cursorStore.commonDocumentsFolderCursor.cursor;
            timestamp = _cursorStore.commonDocumentsFolderCursor.timestamp;
        }
        inline void sharedFolderCursor(Cursor &listingCursor, Timestamp &timestamp) const {
            listingCursor = _cursorStore.sharedFolderCursor.cursor;
            timestamp = _cursorStore.sharedFolderCursor.timestamp;
        }

    private:
        SyncDbId _dbId{0};
        DriveDbId _driveDbId{0};
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

        CursorStore _cursorStore;
};

} // namespace KDC
