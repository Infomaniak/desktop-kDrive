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
#include "jobs/network/kDrive_API/apitranslator.h"
#include "jobs/network/jobexceptions.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/jsonparserutility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

// Results per page. The API returns 10 without it; the iOS app requests 200 on the same endpoint.
static constexpr auto searchPageSize = "50";

SearchJob::SearchJob(const DriveDbId driveDbId, const SyncDbId syncDbId, std::string searchString, Cursor cursorInput /*= {}*/) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, driveDbId, 0),
    _searchString(std::move(searchString)),
    _cursorInput(std::move(cursorInput)) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;


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
    // The target path of an advanced sync is already expressed in the synchronized tree, e.g. "/Common documents/Project".
    _syncTargetPath = sync.targetPath().relative_path();
}

SearchJob::SearchJob(const DriveDbId driveDbId, std::string searchString, Cursor cursorInput /*= {}*/) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, driveDbId, 0),
    _searchString(std::move(searchString)),
    _cursorInput(std::move(cursorInput)) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
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
    uri.addQueryParameter("limit", searchPageSize);
    if (!_cursorInput.empty()) {
        uri.addQueryParameter("cursor", _cursorInput);
    }

    uri.addQueryParameter("with", "path");
    uri.addQueryParameter("order", "desc");
}


ExitInfo SearchJob::getLocalProperties(const SyncPath &itemPath, LocalProperties &localProperties) const {
    localProperties.path = itemPath;

    if (_syncRootPath.empty()) return ExitCode::Ok; // If sync root path is not set, skip local properties check.

    // The API returns paths of the v3 drive tree ("/Private/...", "/Common documents/...", "/Shared/..."): translate them to
    // the synchronized tree, where the private space is the root while "Common documents" and "Shared" stay folders.
    localProperties.path = localProperties.path.relative_path();
    ApiTranslator::translateV3ToV2(localProperties.path);

    // An advanced sync only mirrors its target folder: results outside of it are never available locally, and results
    // inside of it are located relative to the target folder. The target folder itself is the local sync root.
    if (!_syncTargetPath.empty()) {
        if (!CommonUtility::isDescendantOrEqual(localProperties.path, _syncTargetPath)) {
            return ExitCode::Ok;
        }

        localProperties.path = localProperties.path.lexically_relative(_syncTargetPath);
        if (localProperties.path == SyncPath(Str("."))) {
            localProperties.path.clear();
        }
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
    // AbstractNetworkJob::runJob() sends the whole request again after a failed response: start from scratch each time.
    _searchResults.clear();

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

    // Only a local metadata failure reaches this point: sending the request again would fail the same way.
    if (!exitInfo) {
        disableRetry();
    }

    return exitInfo;
}
} // namespace KDC
