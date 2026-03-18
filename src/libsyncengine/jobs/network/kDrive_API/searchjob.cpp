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
#include "libcommonserver/utility/jsonparserutility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

static constexpr auto privateFolder = "Private/";
static constexpr auto sharedFolder = "Shared/";

SearchJob::SearchJob(DriveDbId driveDbId, SyncDbId syncDbId, const std::string &searchString,
                     const std::string &cursorInput /*= {}*/) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0),
    _searchString(searchString),
    _cursorInput(cursorInput) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
    _apiVersion = 3;

    if (!ParmsDb::instance()) {
        assert(false);
        LOG_WARN(_logger, "ParmsDb must be initialized!");
        throw DbError("ParmsDb must be initialized!");
    }

    bool found = false;
    Sync sync;
    if (!ParmsDb::instance()->selectSync(syncDbId, sync, found)) {
        LOG_WARN(_logger, "Failed to retrieve sync info for syncDbId: " << syncDbId);
        return;
    }

    if (!found) {
        LOG_WARN(_logger, "No sync info found for syncDbId: " << syncDbId);
        return;
    }

    _syncRootPath = sync.localPath();
}

SearchJob::SearchJob(DriveDbId driveDbId, const std::string &searchString, const std::string &cursorInput /*= {}*/) :
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

    uri.addQueryParameter("with", "path");
}

ExitInfo SearchJob::handleResponse(std::istream &is) {
    if (const auto exitInfo = AbstractTokenNetworkJob::handleResponse(is); !exitInfo) {
        return exitInfo;
    }
    if (!jsonRes()) {
        LOG_WARN(_logger, "Invalid JSON object");
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    }

    if (!JsonParserUtility::extractValue(jsonRes(), cursorKey, _cursorOutput)) {
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    }
    if (!JsonParserUtility::extractValue(jsonRes(), hasMoreKey, _hasMore)) {
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    }

    const auto dataArray = jsonRes()->getArray(dataKey);
    if (!dataArray) {
        LOG_WARN(_logger, "Missing data array for search string:" << _searchString);
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    }
    for (auto it = dataArray->begin(); it != dataArray->end(); ++it) {
        const auto obj = it->extract<Poco::JSON::Object::Ptr>();
        NodeId nodeId;
        if (!JsonParserUtility::extractValue(obj, idKey, nodeId)) {
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }
        SyncName name;
        if (!JsonParserUtility::extractValue(obj, nameKey, name)) {
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }
        std::string type;
        if (!JsonParserUtility::extractValue(obj, typeKey, type)) {
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }

        std::string path;
        if (!JsonParserUtility::extractValue(obj, pathKey, path)) {
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }

        SyncTime modifiedTime = 0;
        if (!JsonParserUtility::extractValue(obj, lastModifiedAtKey, modifiedTime, false)) {
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }

        size_t size = 0;
        if (!JsonParserUtility::extractValue(obj, sizeKey, size, false)) {
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }

        bool isAvailableLocally = false;

        if (!_syncRootPath.empty()) {
            if (path.starts_with('/') || path.starts_with('\\')) {
                path.erase(0, 1);
            }

            if (path.starts_with(privateFolder)) {
                path.erase(0, std::char_traits<char>::length(privateFolder));
            } else if (path.starts_with(sharedFolder)) {
                path.erase(0, std::char_traits<char>::length(sharedFolder));
            }

            SyncPath absolutePath = _syncRootPath / path;
            IoError ioError = IoError::Success;
            if (bool res = IoHelper::checkIfPathExists(absolutePath, isAvailableLocally, ioError,
                                                       IoHelper::PathCheckOption::Insensitive);
                !res) {
                LOGW_WARN(_logger,
                          L"IoHelper::checkIfPathExists failed for " << Utility::formatSyncPath(path) << L", error: " << ioError);
            }
        }

        (void) _searchResults.emplace_back(nodeId, name, type == "dir" ? NodeType::Directory : NodeType::File, path, modifiedTime,
                                           size, isAvailableLocally);
    }
    return ExitCode::Ok;
}
} // namespace KDC
