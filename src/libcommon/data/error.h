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

#include "libcommon/utility/types.h"
#include "libcommon/utility/utility.h"

#include <QDataStream>
#include <QList>
#include <Poco/Dynamic/Struct.h>

#include <string>
#include <filesystem>

namespace KDC {

class Error {
    public:
        Error() = default;

        // Error level Server constructor
        Error(const std::string &functionName, ExitCode exitCode, ExitCause exitCause);
        Error(const std::string &functionName, const ExitInfo &exitInfo);

        // Error level SyncPal constructor
        Error(SyncDbId syncDbId, const std::string &workerName, ExitCode exitCode, ExitCause exitCause);
        Error(SyncDbId syncDbId, const std::string &workerName, const ExitInfo &exitInfo);

        // Error level Node constructor
        explicit Error(SyncDbId syncDbId, const NodeId &localNodeId, const NodeId &remoteNodeId, NodeType nodeType,
                       const std::filesystem::path &path, ConflictType conflictType,
                       InconsistencyType inconsistencyType = InconsistencyType::None, CancelType cancelType = CancelType::None,
                       const std::filesystem::path &destinationPath = std::filesystem::path(),
                       ExitCode exitCode = ExitCode::Unknown, ExitCause exitCause = ExitCause::Unknown);

        // Full constructor
        Error(ErrorDbId dbId, int64_t time, ErrorLevel level, const std::string &functionName, SyncDbId syncDbId,
              const std::string &workerName, ExitCode exitCode, ExitCause exitCause, const NodeId &localNodeId,
              const NodeId &remoteNodeId, NodeType nodeType, const std::filesystem::path &path, ConflictType conflictType,
              InconsistencyType inconsistencyType = InconsistencyType::None, CancelType cancelType = CancelType::None,
              const std::filesystem::path &destinationPath = std::filesystem::path());

        [[nodiscard]] ErrorDbId dbId() const { return _dbId; }
        void setDbId(const ErrorDbId dbId) { _dbId = dbId; }
        [[nodiscard]] int64_t time() const { return _time; }
        void setTime(const int64_t time) { _time = time; }
        [[nodiscard]] ErrorLevel level() const { return _level; }
        void setLevel(const ErrorLevel level) { _level = level; }
        [[nodiscard]] const std::string &functionName() const { return _functionName; }
        void setFunctionName(const std::string &val) { _functionName = val; }
        [[nodiscard]] SyncDbId syncDbId() const { return _syncDbId; }
        void setSyncDbId(const SyncDbId val) { _syncDbId = val; }
        [[nodiscard]] const std::string &workerName() const { return _workerName; }
        void setWorkerName(const std::string &val) { _workerName = val; }
        [[nodiscard]] ExitCode exitCode() const { return _exitCode; }
        void setExitCode(const ExitCode val) { _exitCode = val; }
        [[nodiscard]] ExitCause exitCause() const { return _exitCause; }
        void setExitCause(const ExitCause val) { _exitCause = val; }
        [[nodiscard]] const NodeId &localNodeId() const { return _localNodeId; }
        void setLocalNodeId(const NodeId &val) { _localNodeId = val; }
        [[nodiscard]] const NodeId &remoteNodeId() const { return _remoteNodeId; }
        void setRemoteNodeId(const NodeId &val) { _remoteNodeId = val; }
        [[nodiscard]] NodeType nodeType() const { return _nodeType; }
        void setNodeType(const NodeType val) { _nodeType = val; }
        [[nodiscard]] const std::filesystem::path &path() const { return _path; }
        void setPath(const std::filesystem::path &val) { _path = val; }
        [[nodiscard]] const std::filesystem::path &destinationPath() const { return _destinationPath; }
        void setDestinationPath(const std::filesystem::path &val) { _destinationPath = val; }
        [[nodiscard]] ConflictType conflictType() const { return _conflictType; }
        void setConflictType(const ConflictType val) { _conflictType = val; }
        [[nodiscard]] InconsistencyType inconsistencyType() const { return _inconsistencyType; }
        void setInconsistencyType(const InconsistencyType val) { _inconsistencyType = val; }
        [[nodiscard]] CancelType cancelType() const { return _cancelType; }
        void setCancelType(const CancelType val) { _cancelType = val; }

        std::string errorString() const;
        bool isSimilarTo(const Error &other) const;

        void toDynamicStruct(Poco::DynamicStruct &dstruct) const;
        void fromDynamicStruct(const Poco::DynamicStruct &dstruct);

        bool isAutoResolved() const;

        /// TODO : to be removed once we moved to the new GUI ///
        friend void operator>>(QDataStream &in, Error &error) {
            qint64 dbId = 0;
            qint64 time = 0;
            qint64 syncDbId = 0;
            QString functionName;
            QString workerName;
            QString localNodeId;
            QString remoteNodeId;
            QString path;
            QString destinationPath;

            in >> dbId >> time >> error._level >> functionName >> syncDbId >> workerName >> error._exitCode >> error._exitCause >>
                    localNodeId >> remoteNodeId >> error._nodeType >> path >> destinationPath >> error._conflictType >>
                    error._inconsistencyType >> error._cancelType;

            error._dbId = static_cast<ErrorDbId>(dbId);
            error._time = static_cast<int64_t>(time);
            error._functionName = functionName.toStdString();
            error._syncDbId = static_cast<SyncDbId>(syncDbId);
            error._workerName = workerName.toStdString();
            error._localNodeId = localNodeId.toStdString();
            error._remoteNodeId = remoteNodeId.toStdString();
            error._path = QStr2Path(path);
            error._destinationPath = QStr2Path(destinationPath);
        }
        friend QDataStream &operator<<(QDataStream &out, const Error &error) {
            out << static_cast<qint64>(error._dbId) << static_cast<qint64>(error._time) << error._level
                << QString::fromStdString(error._functionName) << static_cast<qint64>(error._syncDbId)
                << QString::fromStdString(error._workerName) << error._exitCode << error._exitCause
                << QString::fromStdString(error._localNodeId) << QString::fromStdString(error._remoteNodeId) << error._nodeType
                << Path2QStr(error._path) << Path2QStr(error._destinationPath) << error._conflictType << error._inconsistencyType
                << error._cancelType;
            return out;
        }

        friend void operator>>(QDataStream &in, QList<Error> &list) {
            qint64 count = 0;
            in >> count;
            for (qint64 i = 0; i < count; i++) {
                Error error;
                in >> error;
                list.push_back(error);
            }
        }
        friend QDataStream &operator<<(QDataStream &out, const QList<Error> &list) {
            const auto count = static_cast<qint64>(list.size());
            out << count;
            for (qint64 i = 0; i < count; i++) {
                out << list[static_cast<qsizetype>(i)];
            }
            return out;
        }
        /////////////////////////////////////////////////////////

        bool operator==(const Error &other) const = default;

    private:
        ErrorDbId _dbId{0};
        int64_t _time{0};
        ErrorLevel _level{ErrorLevel::Unknown};
        std::string _functionName;
        SyncDbId _syncDbId{0};
        std::string _workerName;
        ExitCode _exitCode{ExitCode::Unknown};
        ExitCause _exitCause{ExitCause::Unknown};
        NodeId _localNodeId;
        NodeId _remoteNodeId;
        NodeType _nodeType{NodeType::Unknown};
        std::filesystem::path _path;
        std::filesystem::path _destinationPath;
        ConflictType _conflictType{ConflictType::None};
        InconsistencyType _inconsistencyType{InconsistencyType::None};
        CancelType _cancelType{CancelType::None};
};

using ErrorList = std::vector<Error>;

} // namespace KDC
