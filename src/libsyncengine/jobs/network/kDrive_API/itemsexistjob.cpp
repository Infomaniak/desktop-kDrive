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

#include "itemsexistjob.h"

#include "utility/jsonparserutility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

ItemsExistJob::ItemsExistJob(const DriveDbId driveDbId, const NodeSet &ids) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_POST;
    for (const auto &id: ids) {
        (void) _nodeExistenceMap.try_emplace(id, false);
    }
}

bool ItemsExistJob::exists(const NodeId &id, IoError &ioError) {
    if (!_nodeExistenceMap.contains(id)) {
        ioError = IoError::InvalidArgument;
        return false;
    }
    if (_nodeExistenceMap[id]) {
        ioError = IoError::Success;
        return true;
    }
    ioError = IoError::NoSuchFileOrDirectory;
    return false;
}

ExitInfo ItemsExistJob::setData() {
    Poco::JSON::Object obj;
    Poco::JSON::Array ids;
    for (const auto &[id, exists]: _nodeExistenceMap) {
        (void) ids.add(id);
    }
    (void) obj.set("ids", ids);

    std::stringstream ss;
    try {
        obj.stringify(ss);
    } catch (Poco::Exception &e) {
        LOG_ERROR(_logger, "Failed to convert JSON object to string: " << e.message());
        return ExitCode::LogicError;
    }

    _data = ss.str();
    return ExitCode::Ok;
}

std::string ItemsExistJob::getSpecificUrl() {
    return AbstractTokenNetworkJob::getSpecificUrl() + "/files/exists";
}

ExitInfo ItemsExistJob::handleResponse(std::istream &is) {
    if (const auto exitCode = AbstractTokenNetworkJob::handleResponse(is); !exitCode) return exitCode;
    if (!jsonRes()) {
        LOG_ERROR(_logger, "No valid JSON object.");
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    }

    const auto dataArray = jsonRes()->getArray(dataKey);
    if (!dataArray) {
        LOG_ERROR(_logger, "Missing data array, key:" << dataKey);
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    }
    for (auto it = dataArray->begin(); it != dataArray->end(); ++it) {
        const auto obj = it->extract<Poco::JSON::Object::Ptr>();
        NodeId nodeId;
        if (!JsonParserUtility::extractValue(obj, idKey, nodeId)) {
            LOG_WARN(_logger, "Missing object:" << idKey);
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }
        bool exists = false;
        if (!JsonParserUtility::extractValue(obj, resultKey, exists)) {
            LOG_WARN(_logger, "Missing object:" << resultKey);
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }
        if (!_nodeExistenceMap.contains(nodeId)) {
            LOG_WARN(_logger, "ID " << nodeId << " is not watched.");
            continue;
        }

        _nodeExistenceMap[nodeId] = exists;
    }
    return ExitCode::Ok;
}

} // namespace KDC
