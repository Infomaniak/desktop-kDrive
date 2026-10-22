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

#include "data/syncfileitem.h"

#include "utility/utility.h"

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

SyncFileItem::SyncFileItem(NodeType type, const SyncPath &path, const std::optional<SyncPath> &newPath,
                           const std::optional<NodeId> &localNodeId, const std::optional<NodeId> &remoteNodeId,
                           SyncDirection direction, SyncFileInstruction instruction, SyncFileStatus status, ConflictType conflict,
                           InconsistencyType inconsistency, CancelType cancelType, int64_t size, bool dehydrated) :
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
    _cancelType(cancelType),
    _size(size),
    _dehydrated(dehydrated) {}

SyncFileItem::SyncFileItem(NodeType type, const SyncPath &path, const std::optional<NodeId> &localNodeId,
                           const std::optional<NodeId> &remoteNodeId, SyncDirection direction, SyncFileInstruction instruction,
                           ConflictType conflict, int64_t size) :
    _type(type),
    _path(path),
    _localNodeId(localNodeId),
    _remoteNodeId(remoteNodeId),
    _direction(direction),
    _instruction(instruction),
    _conflict(conflict),
    _size(size) {}

void SyncFileItem::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, outParamsType, _type);
    CommonUtility::writeValueToStruct(dstruct, outParamsPath, Path2QStr(_path).toStdString());
    CommonUtility::writeValueToStruct(dstruct, outParamsNewPath,
                                      _newPath.has_value() ? Path2QStr(_newPath.value()).toStdString() : std::string());
    CommonUtility::writeValueToStruct(dstruct, outParamsLocalNodeId,
                                      _localNodeId.has_value() ? _localNodeId.value() : std::string());
    CommonUtility::writeValueToStruct(dstruct, outParamsRemoteNodeId,
                                      _remoteNodeId.has_value() ? _remoteNodeId.value() : std::string());
    CommonUtility::writeValueToStruct(dstruct, outParamsDirection, _direction);
    CommonUtility::writeValueToStruct(dstruct, outParamsInstruction, _instruction);
    CommonUtility::writeValueToStruct(dstruct, outParamsStatus, _status);
    CommonUtility::writeValueToStruct(dstruct, outParamsConflict, _conflict);
    CommonUtility::writeValueToStruct(dstruct, outParamsInconsistency, _inconsistency);
    CommonUtility::writeValueToStruct(dstruct, outParamsCancelType, _cancelType);
    CommonUtility::writeValueToStruct(dstruct, outParamsError, _error);
    CommonUtility::writeValueToStruct(dstruct, outParamsSize, _size);
    CommonUtility::writeValueToStruct(dstruct, outParamsProgress, _progress);
    CommonUtility::writeValueToStruct(dstruct, outParamsOperationId, _operationId);
}

} // namespace KDC
