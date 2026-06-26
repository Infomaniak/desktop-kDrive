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

#include "utility/types.h"

#include <QDataStream>
#include <QList>
#include <string>
#include <filesystem>

#include <Poco/Dynamic/Struct.h>

namespace KDC {

class Sync {
    public:
        Sync() = default;
        Sync(SyncDbId dbId, DriveDbId driveDbId, const std::filesystem::path &localPath, const NodeId &localNodeId,
             const std::filesystem::path &targetPath, const NodeId &targetNodeId = NodeId(), bool paused = false,
             bool supportVfs = false, VirtualFileMode virtualFileMode = VirtualFileMode::Off, bool notificationsDisabled = false,
             const std::filesystem::path &dbPath = std::filesystem::path(), bool hasFullyCompleted = false,
             const std::string &navigationPaneClsid = std::string(), const std::string &listingCursor = std::string(),
             int64_t listingCursorTimestamp = 0);

        void setDbId(SyncDbId dbId) { _dbId = dbId; }
        [[nodiscard]] SyncDbId dbId() const { return _dbId; }
        void setDriveDbId(DriveDbId driveDbId) { _driveDbId = driveDbId; }
        [[nodiscard]] DriveDbId driveDbId() const { return _driveDbId; }
        void setLocalPath(const std::filesystem::path &localPath) { _localPath = localPath; }
        [[nodiscard]] const std::filesystem::path &localPath() const { return _localPath; }
        void setLocalNodeId(const NodeId &localNodeId) { _localNodeId = localNodeId; }
        [[nodiscard]] const NodeId &localNodeId() const { return _localNodeId; }
        void setTargetPath(const std::filesystem::path &targetPath) { _targetPath = targetPath; }
        [[nodiscard]] const std::filesystem::path &targetPath() const { return _targetPath; }
        void setTargetNodeId(const NodeId &targetNodeId) { _targetNodeId = targetNodeId; }
        [[nodiscard]] const NodeId &targetNodeId() const { return _targetNodeId; }
        void setDbPath(const std::filesystem::path &dbPath) { _dbPath = dbPath; }
        [[nodiscard]] const std::filesystem::path &dbPath() const { return _dbPath; }
        void setPaused(bool paused) { _paused = paused; }
        [[nodiscard]] bool paused() const { return _paused; }
        void setSupportVfs(bool supportVfs) { _supportVfs = supportVfs; }
        [[nodiscard]] bool supportVfs() const { return _supportVfs; }
        void setVirtualFileMode(VirtualFileMode virtualFileMode) { _virtualFileMode = virtualFileMode; }
        [[nodiscard]] VirtualFileMode virtualFileMode() const { return _virtualFileMode; }
        void setNotificationsDisabled(bool notificationsDisabled) { _notificationsDisabled = notificationsDisabled; }
        [[nodiscard]] bool notificationsDisabled() const { return _notificationsDisabled; }
        void setHasFullyCompleted(bool hasFullyCompleted) { _hasFullyCompleted = hasFullyCompleted; }
        [[nodiscard]] bool hasFullyCompleted() const { return _hasFullyCompleted; }
        void setNavigationPaneClsid(const std::string &navigationPaneClsid) { _navigationPaneClsid = navigationPaneClsid; }
        [[nodiscard]] const std::string &navigationPaneClsid() const { return _navigationPaneClsid; }
        [[nodiscard]] const std::string &listingCursor() const { return _listingCursor; }
        [[nodiscard]] int64_t listingCursorTimestamp() const { return _listingCursorTimestamp; }
        void setListingCursor(const std::string &listingCursor, int64_t timestamp) {
            _listingCursor = listingCursor;
            _listingCursorTimestamp = timestamp;
        }
        void listingCursor(std::string &listingCursor, int64_t &timestamp) const {
            listingCursor = _listingCursor;
            timestamp = _listingCursorTimestamp;
        }

        void toDynamicStruct(Poco::DynamicStruct &dstruct) const;
        void fromDynamicStruct(const Poco::DynamicStruct &dstruct);

        /// TODO : to be removed once we moved to the new GUI ///
        friend void operator>>(QDataStream &in, Sync &sync) {
            qint64 dbId = 0;
            qint64 driveDbId = 0;
            QString localPath;
            QString localNodeId;
            QString targetPath;
            QString targetNodeId;
            bool paused = false;
            bool supportVfs = false;
            VirtualFileMode virtualFileMode = VirtualFileMode::Off;
            bool notificationsDisabled = false;
            QString dbPath;
            bool hasFullyCompleted = false;
            QString navigationPaneClsid;
            QString listingCursor;
            qint64 listingCursorTimestamp = 0;

            in >> dbId >> driveDbId >> localPath >> localNodeId >> targetPath >> targetNodeId >> paused >> supportVfs >>
                    virtualFileMode >> notificationsDisabled >> dbPath >> hasFullyCompleted >> navigationPaneClsid >>
                    listingCursor >> listingCursorTimestamp;

            sync.setDbId(static_cast<SyncDbId>(dbId));
            sync.setDriveDbId(static_cast<DriveDbId>(driveDbId));
            sync.setLocalPath(QStr2Path(localPath));
            sync.setLocalNodeId(localNodeId.toStdString());
            sync.setTargetPath(QStr2Path(targetPath));
            sync.setTargetNodeId(targetNodeId.toStdString());
            sync.setPaused(paused);
            sync.setSupportVfs(supportVfs);
            sync.setVirtualFileMode(virtualFileMode);
            sync.setNotificationsDisabled(notificationsDisabled);
            sync.setDbPath(QStr2Path(dbPath));
            sync.setHasFullyCompleted(hasFullyCompleted);
            sync.setNavigationPaneClsid(navigationPaneClsid.toStdString());
            sync.setListingCursor(listingCursor.toStdString(), listingCursorTimestamp);
        }
        friend QDataStream &operator<<(QDataStream &out, const Sync &sync) {
            out << static_cast<qint64>(sync._dbId) << static_cast<qint64>(sync._driveDbId) << Path2QStr(sync._localPath)
                << QString::fromStdString(sync._localNodeId) << Path2QStr(sync._targetPath)
                << QString::fromStdString(sync._targetNodeId) << sync._paused << sync._supportVfs << sync._virtualFileMode
                << sync._notificationsDisabled << Path2QStr(sync._dbPath) << sync._hasFullyCompleted
                << QString::fromStdString(sync._navigationPaneClsid) << QString::fromStdString(sync._listingCursor)
                << static_cast<qint64>(sync._listingCursorTimestamp);
            return out;
        }

        friend void operator>>(QDataStream &in, QList<Sync> &list) {
            qint64 count = 0;
            in >> count;
            for (qint64 i = 0; i < count; i++) {
                Sync sync;
                in >> sync;
                list.push_back(sync);
            }
        }
        friend QDataStream &operator<<(QDataStream &out, const QList<Sync> &list) {
            const auto count = static_cast<qint64>(list.size());
            out << count;
            for (qint64 i = 0; i < count; i++) {
                out << list[static_cast<qsizetype>(i)];
            }
            return out;
        }
        /////////////////////////////////////////////////////////

        bool operator==(const Sync &other) const = default;

    private:
        SyncDbId _dbId{0};
        DriveDbId _driveDbId{0};
        std::filesystem::path _localPath;
        NodeId _localNodeId;
        std::filesystem::path _targetPath;
        NodeId _targetNodeId;
        bool _paused{false};
        bool _supportVfs{false};
        VirtualFileMode _virtualFileMode{VirtualFileMode::Off};
        bool _notificationsDisabled{false};
        std::filesystem::path _dbPath;
        bool _hasFullyCompleted{false};
        std::string _navigationPaneClsid;
        std::string _listingCursor;
        int64_t _listingCursorTimestamp{0};
};

using SyncList = std::vector<Sync>;

} // namespace KDC
