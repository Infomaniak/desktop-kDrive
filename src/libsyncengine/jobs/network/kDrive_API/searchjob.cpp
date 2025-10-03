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

#include "searchjob.h"

#include "info/searchinfo.h"
#include "jobs/network/abstracttokennetworkjob.h"
#include "utility/jsonparserutility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

SearchJob::SearchJob(int driveDbId, const std::string &searchString, const std::string &cursorInput /*= {}*/) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0),
    _searchString(searchString),
    _cursorInput(cursorInput) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
    _apiVersion = 3;
}

std::string SearchJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/search/default";
    return str;
}

void SearchJob::setQueryParameters(Poco::URI &uri) {
    if (_searchString.size() > 3) {
        // To search by pattern, the provided string must be at least 3 character long.
        uri.addQueryParameter("query", _searchString);
    } else {
        // Otherwise, search only by name.
        uri.addQueryParameter("name", _searchString);
    }
    uri.addQueryParameter("order_by", "relevance");
    if (!_cursorInput.empty()) {
        uri.addQueryParameter("cursor", _cursorInput);
    }
}

ExitInfo SearchJob::handleResponse(std::istream &is) {
    if (const auto exitInfo = AbstractTokenNetworkJob::handleResponse(is); !exitInfo) {
        return exitInfo;
    }
    if (!jsonRes()) {
        LOG_WARN(_logger, "Invalid JSON object");
        return ExitInfo();
    }

    if (!JsonParserUtility::extractValue(jsonRes(), cursorKey, _cursorOutput)) {
        return ExitInfo();
    }
    if (!JsonParserUtility::extractValue(jsonRes(), hasMoreKey, _hasMore)) {
        return ExitInfo();
    }

    const auto dataArray = jsonRes()->getArray(dataKey);
    if (!dataArray) {
        LOG_WARN(_logger, "Missing data array for search string:" << _searchString);
        return ExitInfo();
    }
    for (auto it = dataArray->begin(); it != dataArray->end(); ++it) {
        const auto obj = it->extract<Poco::JSON::Object::Ptr>();
        NodeId nodeId;
        if (!JsonParserUtility::extractValue(obj, idKey, nodeId)) {
            return ExitInfo();
        }
        SyncName name;
        if (!JsonParserUtility::extractValue(obj, nameKey, name)) {
            return ExitInfo();
        }
        std::string type;
        if (!JsonParserUtility::extractValue(obj, typeKey, type)) {
            return ExitInfo();
        }
        (void) _searchResults.emplace_back(nodeId, name, type == "dir" ? NodeType::Directory : NodeType::File);
    }

    return ExitCode::Ok;
}

} // namespace KDC
