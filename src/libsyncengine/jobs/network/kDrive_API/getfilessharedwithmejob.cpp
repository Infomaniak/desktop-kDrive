/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2026 Infomaniak Network SA
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

#include "jobs/network/kDrive_API/getfilessharedwithmejob.h"

#include "jobs/network/abstracttokennetworkjob.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/jsonparserutility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

GetFilesSharedWithMeJob::GetFilesSharedWithMeJob(const UserDbId userDbId, const DriveId driveId, Cursor cursorInput,
                                                 const int64_t limit /*= 10*/) :
    AbstractTokenNetworkJob(ApiType::Drive, userDbId, 0, driveId),
    _limit(limit),
    _cursorInput(std::move(cursorInput)) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
}

std::string GetFilesSharedWithMeJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/shared_with_me";

    return str;
}

void GetFilesSharedWithMeJob::setQueryParameters(Poco::URI &uri) {
    if (!_cursorInput.empty()) uri.addQueryParameter("cursor", _cursorInput);

    uri.addQueryParameter("limit", std::to_string(_limit));
}

ExitInfo GetFilesSharedWithMeJob::deserializeDataArray() {
    const auto dataArray = jsonRes()->getArray(dataKey);
    if (!dataArray) {
        LOG_WARN(_logger, "Missing data array for GetFilesSharedWithMeJob.");
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    }

    for (auto it = dataArray->begin(); it != dataArray->end(); ++it) {
        const auto obj = it->extract<Poco::JSON::Object::Ptr>();

        RemoteNodeId nodeId;
        if (!JsonParserUtility::extractValue(obj, idKey, nodeId)) return {ExitCode::BackError, ExitCause::MissingReplyData};

        SyncName rawName;
        if (!JsonParserUtility::extractValue(obj, nameKey, rawName)) return {ExitCode::BackError, ExitCause::MissingReplyData};

        SyncName name;
        if (!Utility::normalizedSyncName(rawName, name)) {
            LOGW_DEBUG(_logger, L"Error in Utility::normalizedSyncName: " << Utility::formatSyncName(rawName));
        }
        if (name.empty()) continue;

        RemoteNodeId parentId;
        if (!JsonParserUtility::extractValue(obj, parentIdKey, parentId))
            return {ExitCode::BackError, ExitCause::MissingReplyData};

        std::string typeString;
        if (!JsonParserUtility::extractValue(obj, typeKey, typeString)) {
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }

        qint64 fileSize = -1;
        if (typeString != dirKey && !JsonParserUtility::extractValue(obj, sizeKey, fileSize, false)) {
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }

        SyncTime modifiedTime = 0;
        if (const bool mandatory = false; !JsonParserUtility::extractValue(obj, lastModifiedAtKey, modifiedTime, mandatory)) {
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }

        NodeInfo nodeInfo(QString::fromStdString(nodeId), SyncName2QStr(name), fileSize, parentId.c_str(), modifiedTime);
        (void) _remoteNodeInfoList.emplace_back(std::move(nodeInfo));
    }

    return ExitCode::Ok;
}

ExitInfo GetFilesSharedWithMeJob::handleResponse(std::istream &is) {
    // AbstractNetworkJob::runJob() sends the whole request again after a failed response: start from scratch each time.
    _remoteNodeInfoList.clear();

    if (const auto exitInfo = AbstractTokenNetworkJob::handleResponse(is); !exitInfo) return exitInfo;

    if (!jsonRes()) {
        LOG_WARN(_logger, "Invalid JSON object");
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    }

    if (!JsonParserUtility::extractValue(jsonRes(), cursorKey, _cursorOutput))
        return {ExitCode::BackError, ExitCause::MissingReplyData};

    if (!JsonParserUtility::extractValue(jsonRes(), hasMoreKey, _hasMore))
        return {ExitCode::BackError, ExitCause::MissingReplyData};

    return deserializeDataArray();
}

} // namespace KDC
