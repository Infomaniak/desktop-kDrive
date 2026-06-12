/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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
             const std::string &navigationPaneClsid = std::string(), CursorStore cursorStore = defaultCursorStore);

        void setDbId(const SyncDbId dbId) { _dbId = dbId; }
        SyncDbId dbId() const { return _dbId; }
        void setDriveDbId(const DriveDbId driveDbId) { _driveDbId = driveDbId; }
        DriveDbId driveDbId() const { return _driveDbId; }
        void setLocalPath(const SyncPath &localPath) { _localPath = localPath; }
        const SyncPath &localPath() const { return _localPath; }
        void setLocalNodeId(const NodeId &localNodeId) { _localNodeId = localNodeId; }
        const NodeId &localNodeId() const { return _localNodeId; }
        void setTargetPath(const SyncPath &targetPath) { _targetPath = targetPath; }
        const SyncPath &targetPath() const { return _targetPath; }
        void setTargetNodeId(const NodeId &targetNodeId) { _targetNodeId = targetNodeId; }
        const RemoteNodeId &targetNodeId() const { return _targetNodeId; }
        void setDbPath(const SyncPath &dbPath) { _dbPath = dbPath; }
        const SyncPath &dbPath() const { return _dbPath; }
        void setPaused(const bool paused) { _paused = paused; }
        bool paused() const { return _paused; }
        void setSupportVfs(const bool supportVfs) { _supportVfs = supportVfs; }
        bool supportVfs() const { return _supportVfs; }
        void setVirtualFileMode(const VirtualFileMode virtualFileMode) { _virtualFileMode = virtualFileMode; }
        VirtualFileMode virtualFileMode() const { return _virtualFileMode; }
        void setNotificationsDisabled(const bool notificationsDisabled) { _notificationsDisabled = notificationsDisabled; }
        bool notificationsDisabled() const { return _notificationsDisabled; }
        void setHasFullyCompleted(const bool hasFullyCompleted) { _hasFullyCompleted = hasFullyCompleted; }
        bool hasFullyCompleted() const { return _hasFullyCompleted; }
        void setNavigationPaneClsid(const std::string &navigationPaneClsid) { _navigationPaneClsid = navigationPaneClsid; }
        const std::string &navigationPaneClsid() const { return _navigationPaneClsid; }

        [[nodiscard]] const CursorStore &getCursorStore() const { return _cursorStore; };
        void setCursorStore(const CursorStore &cursors) {
            _cursorStore = cursors;
            for (const auto specialFolder: {SpecialRemoteFolder::Private, SpecialRemoteFolder::CommonDocuments,
                                            SpecialRemoteFolder::Shared, SpecialRemoteFolder::CustomTarget}) {
                if (!_cursorStore.contains(specialFolder)) _cursorStore[specialFolder] = {};
            }
        }

        void setFolderCursor(const SpecialRemoteFolder specialFolder, const CursorData &cursorData) {
            _cursorStore[specialFolder] = cursorData;
        }

        void getFolderCursor(const SpecialRemoteFolder specialFolder, CursorData &cursorData) const {
            if (!_cursorStore.contains(specialFolder)) {
                cursorData = {};
            } else
                cursorData = _cursorStore.at(specialFolder);
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

        CursorStore _cursorStore = defaultCursorStore;
};

} // namespace KDC
