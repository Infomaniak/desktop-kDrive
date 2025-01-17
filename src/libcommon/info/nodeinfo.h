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

#include "libcommon/utility/types.h"

#include <QDataStream>
#include <QString>
#include <QList>

namespace KDC {

class NodeInfo {
    public:
        NodeInfo(QString nodeId, QString name, qint64 size, QString parentNodeId, SyncTime modtime, QString path = "");
        NodeInfo();

        inline const QString &nodeId() const { return _nodeId; }
        inline void setNodeId(const QString &newNodeId) { _nodeId = newNodeId; }
        inline const QString &name() const { return _name; }
        inline void setName(const QString &newName) { _name = newName; }
        inline qint64 size() const { return _size; }
        inline void setSize(qint64 newSize) { _size = newSize; }
        inline const QString &parentNodeId() const { return _parentNodeId; }
        inline void setParentNodeId(const QString &newParentNodeId) { _parentNodeId = newParentNodeId; }
        inline qint64 modtime() const { return _modtime; }
        inline void setModtime(qint64 newModtime) { _modtime = newModtime; }
        inline const QString &path() const { return _path; }
        inline void setPath(const QString &newPath) { _path = newPath; }

        friend QDataStream &operator>>(QDataStream &in, NodeInfo &info);
        friend QDataStream &operator<<(QDataStream &out, const NodeInfo &info);

        friend QDataStream &operator>>(QDataStream &in, QList<NodeInfo> &list);
        friend QDataStream &operator<<(QDataStream &out, const QList<NodeInfo> &list);

    private:
        QString _nodeId;
        QString _name;
        qint64 _size;
        QString _parentNodeId;
        qint64 _modtime;
        QString _path;
};

} // namespace KDC
