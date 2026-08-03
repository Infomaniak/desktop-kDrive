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

#include "searchjob.h"

#include "info/searchinfo.h"
#include "jobs/network/abstracttokennetworkjob.h"
#include "jobs/network/jobexceptions.h"
#include "libcommonserver/utility/jsonparserutility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

static constexpr auto privateFolder = Str("/Private/");
static constexpr auto sharedFolder = Str("/Shared/");

SearchJob::SearchJob(const DriveDbId driveDbId, const SyncDbId syncDbId, const std::string &searchString,
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

    _syncVfsMode = sync.virtualFileMode();
    _syncRootPath = sync.localPath();
}

SearchJob::SearchJob(const DriveDbId driveDbId, const std::string &searchString, const std::string &cursorInput /*= {}*/) :
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


ExitInfo SearchJob::getLocalProperties(const SyncPath &itemPath, LocalProperties &localProperties) const {
    localProperties.path = itemPath;

    if (_syncRootPath.empty()) return ExitCode::Ok; // If sync root path is not set, skip local properties check.

    if (localProperties.path.native().starts_with(privateFolder)) {
        localProperties.path = localProperties.path.native().substr(
                std::char_traits<std::remove_cvref_t<decltype(*privateFolder)>>::length(privateFolder));
    } else if (localProperties.path.native().starts_with(sharedFolder)) {
        localProperties.path = localProperties.path.native().substr(
                std::char_traits<std::remove_cvref_t<decltype(*sharedFolder)>>::length(sharedFolder));
    }

    if (localProperties.path.native().starts_with(Str("/")) || localProperties.path.native().starts_with(Str("\\"))) {
        localProperties.path = localProperties.path.relative_path();
    }

    const SyncPath absolutePath = _syncRootPath / localProperties.path;

    if (_syncVfsMode == VirtualFileMode::Off) {
        if (IoError ioError = IoError::Success; !IoHelper::checkIfPathExists(absolutePath, localProperties.isAvailableLocally,
                                                                             ioError, IoHelper::PathCheckOption::Insensitive)) {
            LOGW_WARN(_logger, L"IoHelper::checkIfPathExists failed for " << Utility::formatIoError(itemPath, ioError));
            return {ExitCode::SystemError, ExitCause::FileAccessError};
        }
        localProperties.isHydrated = localProperties.isAvailableLocally;
    } else {
        IoError ioError = IoError::Success;
        bool isDehydrated = false;
        if (!IoHelper::checkIfFileIsDehydrated(absolutePath, isDehydrated, ioError) ||
            (ioError != IoError::Success && ioError != IoError::NoSuchFileOrDirectory)) {
            LOGW_WARN(_logger, L"IoHelper::checkIfFileIsDehydrated failed for " << Utility::formatIoError(itemPath, ioError));
            return {ExitCode::SystemError, ExitCause::FileAccessError};
        } else {
            localProperties.isAvailableLocally = true;
            localProperties.isHydrated = !isDehydrated;
        }
    }

    return ExitCode::Ok;
}

ExitInfo SearchJob::handleResponse(std::istream &is) {
    if (const auto exitInfo = AbstractTokenNetworkJob::handleResponse(is); !exitInfo) return exitInfo;

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

    ExitInfo exitInfo = ExitCode::Ok;

    for (auto it = dataArray->begin(); it != dataArray->end(); ++it) {
        const auto obj = it->extract<Poco::JSON::Object::Ptr>();
        RemoteNodeId nodeId;
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

        SyncName pathStr;
        if (!JsonParserUtility::extractValue(obj, pathKey, pathStr)) {
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }
        SyncPath path(pathStr);

        SyncTime modifiedTime = 0;
        if (!JsonParserUtility::extractValue(obj, lastModifiedAtKey, modifiedTime, false)) {
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }

        size_t size = 0;
        if (!JsonParserUtility::extractValue(obj, sizeKey, size, false)) {
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }

        LocalProperties localProperties;
        const auto itemExitInfo = getLocalProperties(path, localProperties);
        (void) _searchResults.emplace_back(nodeId, name, type == "dir" ? NodeType::Directory : NodeType::File,
                                           localProperties.path, modifiedTime, size, localProperties.isAvailableLocally,
                                           localProperties.isHydrated);

        if (!itemExitInfo) exitInfo = itemExitInfo; // Stores only the last error for the final return value.
    }

    return exitInfo;
}
} // namespace KDC
