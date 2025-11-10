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

#include "nodeinfo.h"

namespace KDC {

NodeInfo::NodeInfo(QString nodeId, QString name, qint64 size, QString parentNodeId, SyncTime modtime, QString path /*= ""*/) :
    _nodeId(nodeId),
    _name(name),
    _size(size),
    _parentNodeId(parentNodeId),
    _modtime(modtime),
    _path(path) {}

NodeInfo::NodeInfo() :
    _nodeId(QString()),
    _name(QString()),
    _size(0),
    _parentNodeId(QString()),
    _modtime(0) {}

QDataStream &operator>>(QDataStream &in, NodeInfo &info) {
    in >> info._nodeId >> info._name >> info._size >> info._parentNodeId >> info._modtime >> info._path;
    return in;
}

QDataStream &operator<<(QDataStream &out, const NodeInfo &info) {
    out << info._nodeId << info._name << info._size << info._parentNodeId << info._modtime << info._path;
    return out;
}

QDataStream &operator<<(QDataStream &out, const QList<NodeInfo> &list) {
    int count = static_cast<int>(list.size());
    out << count;
    for (int i = 0; i < count; i++) {
        NodeInfo info = list[i];
        out << info;
    }
    return out;
}

QDataStream &operator>>(QDataStream &in, QList<NodeInfo> &list) {
    auto count = 0;
    in >> count;
    for (int i = 0; i < count; i++) {
        NodeInfo info;
        in >> info;
        list.push_back(info);
    }
    return in;
}

} // namespace KDC
