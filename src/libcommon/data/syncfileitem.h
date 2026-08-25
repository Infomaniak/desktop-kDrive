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

#include <Poco/Dynamic/Struct.h>
#include <QDataStream>
#include <QList>

#include <optional>

namespace KDC {

class SyncFileItem {
    public:
        SyncFileItem() = default;
        SyncFileItem(NodeType type, const SyncPath &path, const std::optional<SyncPath> &newPath,
                     const std::optional<NodeId> &localNodeId, const std::optional<NodeId> &remoteNodeId, SyncDirection direction,
                     SyncFileInstruction instruction, SyncFileStatus status, ConflictType conflict,
                     InconsistencyType inconsistency, CancelType cancelType, int64_t size, bool dehydrated);
        SyncFileItem(NodeType type, const SyncPath &path, const std::optional<NodeId> &localNodeId,
                     const std::optional<NodeId> &remoteNodeId, SyncDirection direction, SyncFileInstruction instruction,
                     ConflictType conflict, int64_t size);

        inline NodeType type() const { return _type; }
        inline void setType(const NodeType newType) { _type = newType; }
        inline const SyncPath &path() const { return _path; }
        inline void setPath(const SyncPath &newPath) { _path = newPath; }
        inline std::optional<SyncPath> newPath() const { return _newPath; }
        inline void setNewPath(const std::optional<SyncPath> newNewPath) { _newPath = newNewPath; }
        inline std::optional<NodeId> localNodeId() const { return _localNodeId; }
        inline void setLocalNodeId(const std::optional<NodeId> newLocalNodeId) { _localNodeId = newLocalNodeId; }
        inline std::optional<NodeId> remoteNodeId() const { return _remoteNodeId; }
        inline void setRemoteNodeId(const std::optional<NodeId> newRemoteNodeId) { _remoteNodeId = newRemoteNodeId; }
        inline SyncDirection direction() const { return _direction; }
        inline void setDirection(const SyncDirection newDirection) { _direction = newDirection; }
        inline SyncFileInstruction instruction() const { return _instruction; }
        inline void setInstruction(const SyncFileInstruction newInstruction) { _instruction = newInstruction; }
        inline SyncFileStatus status() const { return _status; }
        inline void setStatus(const SyncFileStatus newStatus) { _status = newStatus; }
        inline ConflictType conflict() const { return _conflict; }
        inline void setConflict(const ConflictType newConflict) { _conflict = newConflict; }
        inline InconsistencyType inconsistency() const { return _inconsistency; }
        inline void setInconsistency(const InconsistencyType newInconsistency) { _inconsistency = newInconsistency; }
        inline CancelType cancelType() const { return _cancelType; }
        inline void setCancelType(const CancelType newCancelType) { _cancelType = newCancelType; }
        inline std::string error() const { return _error; }
        inline void setError(const std::string &error) { _error = error; }
        inline int64_t size() const { return _size; }
        inline void setSize(const int64_t newSize) { _size = newSize; }
        inline int progress() const { return _progress; }
        inline void setProgress(const int16_t newProgress) { _progress = newProgress; }
        inline UniqueId operationId() const { return _operationId; }
        inline void setOperationId(const UniqueId newOperationId) { _operationId = newOperationId; }
        inline SyncTime modTime() const { return _modTime; }
        inline void setModTime(const SyncTime newModTime) { _modTime = newModTime; }
        inline SyncTime creationTime() const { return _creationTime; }
        inline void setCreationTime(const SyncTime newCreationTime) { _creationTime = newCreationTime; }
        inline bool dehydrated() const { return _dehydrated; }
        inline void setDehydrated(const bool newDehydrated) { _dehydrated = newDehydrated; }
        inline SyncTime timestamp() const { return _timestamp; }
        inline void setTimestamp(const SyncTime newTimestamp) { _timestamp = newTimestamp; }

        inline bool isDirectory() const { return _type == NodeType::Directory; }

        void toDynamicStruct(Poco::DynamicStruct &dstruct) const;

        /// TODO : to be removed once we moved to the new GUI ///
        friend QDataStream &operator>>(QDataStream &in, SyncFileItem &info) {
            QString path;
            QString newPath;
            QString localNodeId;
            QString remoteNodeId;

            in >> info._type >> path >> newPath >> localNodeId >> remoteNodeId >> info._direction >> info._instruction >>
                    info._status >> info._conflict >> info._inconsistency >> info._cancelType;

            info._path = QStr2Path(path);
            info._newPath = newPath.isEmpty() ? std::nullopt : std::optional<SyncPath>(QStr2Path(newPath));
            info._localNodeId = localNodeId.isEmpty() ? std::nullopt : std::optional<NodeId>(localNodeId.toStdString());
            info._remoteNodeId = remoteNodeId.isEmpty() ? std::nullopt : std::optional<NodeId>(remoteNodeId.toStdString());

            return in;
        }
        friend QDataStream &operator<<(QDataStream &out, const SyncFileItem &info) {
            out << info._type << Path2QStr(info._path)
                << (info._newPath.has_value() ? Path2QStr(info._newPath.value()) : QString())
                << (info._localNodeId.has_value() ? QString::fromStdString(info._localNodeId.value()) : QString())
                << (info._remoteNodeId.has_value() ? QString::fromStdString(info._remoteNodeId.value()) : QString())
                << info._direction << info._instruction << info._status << info._conflict << info._inconsistency
                << info._cancelType;

            return out;
        }

        friend QDataStream &operator>>(QDataStream &in, QList<SyncFileItem> &list) {
            qint64 count = 0;
            in >> count;
            for (qint64 i = 0; i < count; i++) {
                SyncFileItem syncFileItem;
                in >> syncFileItem;
                list.push_back(syncFileItem);
            }
            return in;
        }
        friend QDataStream &operator<<(QDataStream &out, const QList<SyncFileItem> &list) {
            const auto count = static_cast<qint64>(list.size());
            out << count;
            for (qint64 i = 0; i < count; i++) {
                const SyncFileItem syncFileItem = list[i];
                out << syncFileItem;
            }
            return out;
        }
        /////////////////////////////////////////////////////////

        bool operator==(const SyncFileItem &other) const = default;

    private:
        NodeType _type{NodeType::Unknown};
        SyncPath _path; // Sync folder relative filesystem path
        std::optional<SyncPath> _newPath{std::nullopt};
        std::optional<NodeId> _localNodeId{std::nullopt};
        std::optional<NodeId> _remoteNodeId{std::nullopt};
        SyncDirection _direction{SyncDirection::Unknown};
        SyncFileInstruction _instruction{SyncFileInstruction::None};
        SyncFileStatus _status{SyncFileStatus::Unknown};
        ConflictType _conflict{ConflictType::None};
        InconsistencyType _inconsistency{InconsistencyType::None};
        CancelType _cancelType{CancelType::None};
        std::string _error;
        int64_t _size{0};
        int16_t _progress{0}; // %
        UniqueId _operationId{0};
        SyncTime _modTime{0};
        SyncTime _creationTime{0};
        bool _dehydrated{false};
        SyncTime _timestamp{std::time(nullptr)};
};

} // namespace KDC
