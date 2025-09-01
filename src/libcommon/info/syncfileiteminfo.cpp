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

#include "syncfileiteminfo.h"

namespace KDC {

SyncFileItemInfo::SyncFileItemInfo(NodeType type, const QString &path, const QString &newPath, const QString &localNodeId,
                                   const QString &remoteNodeId, SyncDirection direction, SyncFileInstruction instruction,
                                   SyncFileStatus status, ConflictType conflict, InconsistencyType inconsistency,
                                   CancelType cancelType) :
    _type(type),
    _path(path),
    _newPath(newPath),
    _localNodeId(localNodeId),
    _remoteNodeId(remoteNodeId),
    _direction(direction),
    _instruction(instruction),
    _status(status),
    _conflict(conflict),
    _inconsistency(inconsistency),
    _cancelType(cancelType) {}

SyncFileItemInfo::SyncFileItemInfo() {}

QDataStream &operator>>(QDataStream &in, SyncFileItemInfo &info) {
    in >> info._type >> info._path >> info._newPath >> info._localNodeId >> info._remoteNodeId >> info._direction >>
            info._instruction >> info._status >> info._conflict >> info._inconsistency >> info._cancelType;
    return in;
}

QDataStream &operator<<(QDataStream &out, const SyncFileItemInfo &info) {
    out << info._type << info._path << info._newPath << info._localNodeId << info._remoteNodeId << info._direction
        << info._instruction << info._status << info._conflict << info._inconsistency << info._cancelType;

    return out;
}

QDataStream &operator<<(QDataStream &out, const QList<SyncFileItemInfo> &list) {
    int count = static_cast<int>(list.size());
    out << count;
    for (int i = 0; i < count; i++) {
        SyncFileItemInfo info = list[i];
        out << info;
    }
    return out;
}

QDataStream &operator>>(QDataStream &in, QList<SyncFileItemInfo> &list) {
    int count = 0;
    in >> count;
    for (int i = 0; i < count; i++) {
        SyncFileItemInfo info;
        in >> info;
        list.push_back(info);
    }
    return in;
}

} // namespace KDC
