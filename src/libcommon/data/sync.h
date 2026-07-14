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

class BaseSync {
    public:
        BaseSync() = default;
        BaseSync(SyncDbId dbId, DriveDbId driveDbId, const std::filesystem::path &localPath,
                 const std::filesystem::path &targetPath, const NodeId &targetNodeId = NodeId(), bool supportVfs = false,
                 VirtualFileMode virtualFileMode = VirtualFileMode::Off, const std::string &navigationPaneClsid = std::string());
        BaseSync(const BaseSync &sync) noexcept = default;
        BaseSync(BaseSync &&other) noexcept = default;
        BaseSync &operator=(const BaseSync &other) noexcept = default;
        BaseSync &operator=(BaseSync &&other) noexcept = default;
        virtual ~BaseSync() = default;

        void setDbId(const SyncDbId dbId) { _dbId = dbId; }
        [[nodiscard]] SyncDbId dbId() const { return _dbId; }
        void setDriveDbId(const DriveDbId driveDbId) { _driveDbId = driveDbId; }
        [[nodiscard]] DriveDbId driveDbId() const { return _driveDbId; }
        void setLocalPath(const std::filesystem::path &localPath) { _localPath = localPath; }
        [[nodiscard]] const std::filesystem::path &localPath() const { return _localPath; }
        void setTargetPath(const std::filesystem::path &targetPath) { _targetPath = targetPath; }
        [[nodiscard]] const std::filesystem::path &targetPath() const { return _targetPath; }
        void setTargetNodeId(const NodeId &targetNodeId) { _targetNodeId = targetNodeId; }
        [[nodiscard]] const NodeId &targetNodeId() const { return _targetNodeId; }
        void setSupportVfs(const bool supportVfs) { _supportVfs = supportVfs; }
        [[nodiscard]] bool supportVfs() const { return _supportVfs; }
        void setVirtualFileMode(const VirtualFileMode virtualFileMode) { _virtualFileMode = virtualFileMode; }
        [[nodiscard]] VirtualFileMode virtualFileMode() const { return _virtualFileMode; }
        void setNavigationPaneClsid(const std::string &navigationPaneClsid) { _navigationPaneClsid = navigationPaneClsid; }
        [[nodiscard]] const std::string &navigationPaneClsid() const { return _navigationPaneClsid; }

        virtual void toDynamicStruct(Poco::DynamicStruct &dstruct) const final;
        virtual void fromDynamicStruct(const Poco::DynamicStruct &dstruct) final;

        /// TODO : to be removed once we moved to the new GUI ///
        friend void operator>>(QDataStream &in, BaseSync &sync) {
            qint64 dbId = 0;
            qint64 driveDbId = 0;
            QString localPath;
            QString targetPath;
            QString targetNodeId;
            bool supportVfs = false;
            VirtualFileMode virtualFileMode = VirtualFileMode::Off;
            QString navigationPaneClsid;

            in >> dbId >> driveDbId >> localPath >> targetPath >> targetNodeId >> supportVfs >> virtualFileMode >>
                    navigationPaneClsid;

            sync.setDbId(static_cast<SyncDbId>(dbId));
            sync.setDriveDbId(static_cast<DriveDbId>(driveDbId));
            sync.setLocalPath(QStr2Path(localPath));
            sync.setTargetPath(QStr2Path(targetPath));
            sync.setTargetNodeId(targetNodeId.toStdString());
            sync.setSupportVfs(supportVfs);
            sync.setVirtualFileMode(virtualFileMode);
            sync.setNavigationPaneClsid(navigationPaneClsid.toStdString());
        }
        friend QDataStream &operator<<(QDataStream &out, const BaseSync &sync) {
            out << static_cast<qint64>(sync._dbId) << static_cast<qint64>(sync._driveDbId) << Path2QStr(sync._localPath)
                << Path2QStr(sync._targetPath) << QString::fromStdString(sync._targetNodeId) << sync._supportVfs
                << sync._virtualFileMode << QString::fromStdString(sync._navigationPaneClsid);
            return out;
        }

        friend void operator>>(QDataStream &in, QList<BaseSync> &list) {
            qint64 count = 0;
            in >> count;
            for (qint64 i = 0; i < count; i++) {
                BaseSync sync;
                in >> sync;
                list.push_back(sync);
            }
        }
        friend QDataStream &operator<<(QDataStream &out, const QList<BaseSync> &list) {
            const auto count = static_cast<qint64>(list.size());
            out << count;
            for (qint64 i = 0; i < count; i++) {
                out << list[static_cast<qsizetype>(i)];
            }
            return out;
        }
        /////////////////////////////////////////////////////////

        bool operator==(const BaseSync &other) const = default;

    private:
        SyncDbId _dbId{0};
        DriveDbId _driveDbId{0};
        std::filesystem::path _localPath;
        std::filesystem::path _targetPath;
        NodeId _targetNodeId;
        bool _supportVfs{false};
        VirtualFileMode _virtualFileMode{VirtualFileMode::Off};
        std::string _navigationPaneClsid;
};

class Sync : public BaseSync {
    public:
        Sync() = default;
        Sync(SyncDbId dbId, DriveDbId driveDbId, const std::filesystem::path &localPath, const NodeId &localNodeId,
             const std::filesystem::path &targetPath, const NodeId &targetNodeId = NodeId(), bool paused = false,
             bool supportVfs = false, VirtualFileMode virtualFileMode = VirtualFileMode::Off, bool notificationsDisabled = false,
             const std::filesystem::path &dbPath = std::filesystem::path(), bool hasFullyCompleted = false,
             const std::string &navigationPaneClsid = std::string(), const std::string &listingCursor = std::string(),
             int64_t listingCursorTimestamp = 0);

        [[nodiscard]] const NodeId &localNodeId() const { return _localNodeId; }
        void setLocalNodeId(const NodeId &localNodeId) { _localNodeId = localNodeId; }
        [[nodiscard]] bool paused() const { return _paused; }
        void setPaused(const bool paused) { _paused = paused; }
        [[nodiscard]] bool notificationsDisabled() const { return _notificationsDisabled; }
        void setNotificationsDisabled(const bool notificationsDisabled) { _notificationsDisabled = notificationsDisabled; }
        [[nodiscard]] const std::filesystem::path &dbPath() const { return _dbPath; }
        void setDbPath(const std::filesystem::path &dbPath) { _dbPath = dbPath; }
        [[nodiscard]] bool hasFullyCompleted() const { return _hasFullyCompleted; }
        void setHasFullyCompleted(const bool hasFullyCompleted) { _hasFullyCompleted = hasFullyCompleted; }
        [[nodiscard]] const std::string &listingCursor() const { return _listingCursor; }
        void setListingCursor(const std::string &listingCursor) { _listingCursor = listingCursor; }
        [[nodiscard]] int64_t listingCursorTimestamp() const { return _listingCursorTimestamp; }
        void setListingCursorTimestamp(const int64_t listingCursorTimestamp) { _listingCursorTimestamp = listingCursorTimestamp; }
        void setListingCursor(const std::string &listingCursor, const int64_t timestamp) {
            _listingCursor = listingCursor;
            _listingCursorTimestamp = timestamp;
        }
        void listingCursor(std::string &listingCursor, int64_t &timestamp) const {
            listingCursor = _listingCursor;
            timestamp = _listingCursorTimestamp;
        }

    private:
        NodeId _localNodeId;
        bool _paused{false};
        bool _notificationsDisabled{false};
        std::filesystem::path _dbPath;
        bool _hasFullyCompleted{false};
        std::string _listingCursor;
        int64_t _listingCursorTimestamp{0};
};

} // namespace KDC
