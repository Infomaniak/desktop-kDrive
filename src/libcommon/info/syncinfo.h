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

#include "libcommon/utility/types.h"

#include <QDataStream>
#include <QString>
#include <QList>
#include <QColor>

namespace KDC {

class SyncInfo {
    public:
        SyncInfo() = default;
        SyncInfo(int dbId, int driveDbId, const QString &localPath, const QString &targetPath, const QString &targetNodeId,
                 bool supportVfs, VirtualFileMode virtualFileMode, const QString &navigationPaneClsid);

        inline void setDbId(int dbId) { _dbId = dbId; }
        inline int dbId() const { return _dbId; }
        inline void setDriveDbId(int driveDbId) { _driveDbId = driveDbId; }
        inline int driveDbId() const { return _driveDbId; }
        inline void setLocalPath(const QString &path) { _localPath = path; }
        inline const QString &localPath() const { return _localPath; }
        inline void setTargetPath(const QString &path) { _targetPath = path; }
        inline const QString &targetPath() const { return _targetPath; }
        inline void setTargetNodeId(const QString &nodeId) { _targetNodeId = nodeId; }
        inline const QString &targetNodeId() const { return _targetNodeId; }
        inline void setSupportVfs(bool supportVfs) { _supportVfs = supportVfs; }
        inline bool supportVfs() const { return _supportVfs; }
        inline void setVirtualFileMode(VirtualFileMode virtualFileMode) { _virtualFileMode = virtualFileMode; }
        inline VirtualFileMode virtualFileMode() const { return _virtualFileMode; }
        inline void setNavigationPaneClsid(QString navigationPaneClsid) { _navigationPaneClsid = navigationPaneClsid; }
        inline QString navigationPaneClsid() const { return _navigationPaneClsid; }

        friend QDataStream &operator>>(QDataStream &in, SyncInfo &info);
        friend QDataStream &operator<<(QDataStream &out, const SyncInfo &info);

        friend QDataStream &operator>>(QDataStream &in, QList<SyncInfo> &list);
        friend QDataStream &operator<<(QDataStream &out, const QList<SyncInfo> &list);

    protected:
        int _dbId = 0;
        int _driveDbId = 0;
        QString _localPath;
        QString _targetPath;
        QString _targetNodeId;
        bool _supportVfs = false;
        VirtualFileMode _virtualFileMode = VirtualFileMode::Off;
        QString _navigationPaneClsid;
};

}  // namespace KDC
