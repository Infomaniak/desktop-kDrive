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

#include "fileexistsjob.h"

#include "utility/jsonparserutility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

FileExistsJob::FileExistsJob(int driveDbId, const NodeSet &ids) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_POST;
    for (const auto &id: ids) {
        _itemInfo.emplace(id, false);
    }
}

FileExistsJob::Status FileExistsJob::exists(const NodeId &id) {
    if (!_itemInfo.contains(id)) return UNHANDLED;
    return _itemInfo[id] ? EXISTS : NOT_FOUND;
}

ExitInfo FileExistsJob::setData() {
    Poco::JSON::Object obj;
    Poco::JSON::Array ids;
    for (const auto &[id, exists]: _itemInfo) {
        (void) ids.add(id);
    }
    (void) obj.set("ids", ids);

    std::stringstream ss;
    try {
        obj.stringify(ss);
    } catch (Poco::Exception &e) {
        // Not an issue
        LOG_ERROR(_logger, "Failed to convert JSON object to string: " << e.message());
        return ExitCode::LogicError;
    }

    _data = ss.str();
    return ExitCode::Ok;
}

std::string FileExistsJob::getSpecificUrl() {
    return AbstractTokenNetworkJob::getSpecificUrl() + "/files/exists";
}

ExitInfo FileExistsJob::handleResponse(std::istream &is) {
    if (const auto exitCode = AbstractTokenNetworkJob::handleResponse(is); !exitCode) return exitCode;
    if (!jsonRes()) {
        LOG_ERROR(_logger, "No valid JSON object.");
        return {ExitCode::BackError, ExitCause::InvalidReply};
    }

    const auto dataArray = jsonRes()->getArray(dataKey);
    if (!dataArray) {
        LOG_ERROR(_logger, "Missing data array, key:" << dataKey);
        return {ExitCode::BackError, ExitCause::InvalidReply};
    }
    for (auto it = dataArray->begin(); it != dataArray->end(); ++it) {
        const auto obj = it->extract<Poco::JSON::Object::Ptr>();
        NodeId nodeId;
        if (!JsonParserUtility::extractValue(obj, idKey, nodeId)) {
            LOG_WARN(_logger, "Missing object:" << idKey);
            continue;
        }
        bool exists = false;
        if (!JsonParserUtility::extractValue(obj, resultKey, exists)) {
            LOG_WARN(_logger, "Missing object:" << resultKey);
            continue;
        }
        if (!_itemInfo.contains(nodeId)) {
            LOG_WARN(_logger, "ID " << nodeId << " is not watched.");
            continue;
        }

        _itemInfo[nodeId] = exists;
    }
    return ExitCode::Ok;
}

} // namespace KDC
