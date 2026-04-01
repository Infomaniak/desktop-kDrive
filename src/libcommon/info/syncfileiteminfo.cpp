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

#include "syncfileiteminfo.h"
#include "utility/utility.h"

#include <cstdint>

static const auto outParamsType = "type";
static const auto outParamsPath = "path";
static const auto outParamsNewPath = "newPath";
static const auto outParamsLocalNodeId = "localNodeId";
static const auto outParamsRemoteNodeId = "remoteNodeId";
static const auto outParamsDirection = "direction";
static const auto outParamsInstruction = "instruction";
static const auto outParamsStatus = "status";
static const auto outParamsConflict = "conflict";
static const auto outParamsInconsistency = "inconsistency";
static const auto outParamsCancelType = "cancelType";
static const auto outParamsError = "error";
static const auto outParamsSize = "size";
static const auto outParamsProgress = "progress";
static const auto outParamsOperationId = "operationId";


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

void SyncFileItemInfo::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, outParamsType, _type);
    CommonUtility::writeValueToStruct(dstruct, outParamsPath, _path.toStdString());
    CommonUtility::writeValueToStruct(dstruct, outParamsNewPath, _newPath.toStdString());
    CommonUtility::writeValueToStruct(dstruct, outParamsLocalNodeId, _localNodeId.toStdString());
    CommonUtility::writeValueToStruct(dstruct, outParamsRemoteNodeId, _remoteNodeId.toStdString());
    CommonUtility::writeValueToStruct(dstruct, outParamsDirection, _direction);
    CommonUtility::writeValueToStruct(dstruct, outParamsInstruction, _instruction);
    CommonUtility::writeValueToStruct(dstruct, outParamsStatus, _status);
    CommonUtility::writeValueToStruct(dstruct, outParamsConflict, _conflict);
    CommonUtility::writeValueToStruct(dstruct, outParamsInconsistency, _inconsistency);
    CommonUtility::writeValueToStruct(dstruct, outParamsCancelType, _cancelType);
    CommonUtility::writeValueToStruct(dstruct, outParamsError, _error.toStdString());
    CommonUtility::writeValueToStruct(dstruct, outParamsSize, _size);
    CommonUtility::writeValueToStruct(dstruct, outParamsProgress, _progress);
    CommonUtility::writeValueToStruct(dstruct, outParamsOperationId, _operationId);
}

void SyncFileItemInfo::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    CommonUtility::readValueFromStruct(dstruct, outParamsType, _type);

    CommString path;
    CommonUtility::readValueFromStruct(dstruct, outParamsPath, path);
    _path = CommonUtility::commString2QStr(path);

    CommString newPath;
    CommonUtility::readValueFromStruct(dstruct, outParamsNewPath, newPath);
    _newPath = CommonUtility::commString2QStr(newPath);

    CommString localNodeId;
    CommonUtility::readValueFromStruct(dstruct, outParamsLocalNodeId, localNodeId);
    _localNodeId = CommonUtility::commString2QStr(localNodeId);

    CommString remoteNodeId;
    CommonUtility::readValueFromStruct(dstruct, outParamsRemoteNodeId, remoteNodeId);
    _remoteNodeId = CommonUtility::commString2QStr(remoteNodeId);

    CommonUtility::readValueFromStruct(dstruct, outParamsDirection, _direction);
    CommonUtility::readValueFromStruct(dstruct, outParamsInstruction, _instruction);
    CommonUtility::readValueFromStruct(dstruct, outParamsStatus, _status);
    CommonUtility::readValueFromStruct(dstruct, outParamsConflict, _conflict);
    int32_t inconsistencyValue = 0;
    CommonUtility::readValueFromStruct(dstruct, outParamsInconsistency, inconsistencyValue);
    _inconsistency = fromInt<InconsistencyType>(inconsistencyValue);
    CommonUtility::readValueFromStruct(dstruct, outParamsCancelType, _cancelType);

    CommString error;
    CommonUtility::readValueFromStruct(dstruct, outParamsError, error);
    _error = QString::fromStdString(error);

    CommonUtility::readValueFromStruct(dstruct, outParamsSize, _size);
    CommonUtility::readValueFromStruct(dstruct, outParamsProgress, _progress);
    CommonUtility::readValueFromStruct(dstruct, outParamsOperationId, _operationId);
}

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
