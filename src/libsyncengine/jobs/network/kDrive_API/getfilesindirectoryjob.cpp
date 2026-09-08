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

#include "jobs/network/kDrive_API/getfilesindirectoryjob.h"
#include "jobs/network/kDrive_API/apitranslator.h"

#include "utility/jsonparserutility.h"
#include "jobs/network/jobexceptions.h"


#include <Poco/Net/HTTPRequest.h>

namespace KDC {

void GetFilesInDirectoryJob::translateDriveDbIdFromV2ToV3(const TranslationMode translationMode) {
    if (translationMode != TranslationMode::V2ToV3) return;
    if (const auto exitInfo = ApiTranslator::translateV2ToV3(driveDbId(), _fileId); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ApiTranslator::translateV2ToV3: " << exitInfo);
        throw JobException("Translation error in GetFilesInDirectoryJob::GetFilesInDirectoryJob.");
    }
}

GetFilesInDirectoryJob::GetFilesInDirectoryJob(const UserDbId userDbId, const DriveId driveId, RemoteNodeId fileId,
                                               std::string cursorInput,
                                               const TranslationMode translationMode /* = TranslationMode::V2ToV3 */) :
    AbstractTokenNetworkJob(ApiType::Drive, static_cast<int>(userDbId), 0, 0, static_cast<int>(driveId)),
    _fileId(std::move(fileId)),
    _cursorInput(std::move(cursorInput)) {
    _apiVersion = 3;
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
    translateDriveDbIdFromV2ToV3(translationMode);
}

GetFilesInDirectoryJob::GetFilesInDirectoryJob(const DriveDbId driveDbId, RemoteNodeId fileId, std::string cursorInput,
                                               const TranslationMode translationMode /* = TranslationMode::V2ToV3 */) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, static_cast<int>(driveDbId), 0),
    _fileId(std::move(fileId)),
    _cursorInput(std::move(cursorInput)) {
    _apiVersion = 3;
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
    translateDriveDbIdFromV2ToV3(translationMode);
}

std::string GetFilesInDirectoryJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/";
    str += _fileId;
    str += "/files";

    return str;
}

void GetFilesInDirectoryJob::setQueryParameters(Poco::URI &uri) {
    if (_listingConf.dirOnly) uri.addQueryParameter("type[]", "dir");

    const auto withParameters = _listingConf.withPath ? "path,capabilities" : "capabilities";
    uri.addQueryParameter("with", withParameters);

    if (!_cursorInput.empty()) uri.addQueryParameter("cursor", _cursorInput);

    uri.addQueryParameter("limit", std::to_string(_listingConf.limit));
}

ExitInfo GetFilesInDirectoryJob::deserializeDataArray() {
    const auto dataArray = jsonRes()->getArray(dataKey);
    if (!dataArray) {
        LOG_WARN(_logger, "Missing data array for GetFilesInDirectoryJob.");
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
            LOGW_DEBUG(Log::instance()->getLogger(),
                       L"Error in Utility::normalizedSyncName: " << Utility::formatSyncName(rawName));
            // Ignore the item
            continue;
        }

        SyncName path;
        if (_listingConf.withPath) {
            SyncName rawPath;
            if (!JsonParserUtility::extractValue(obj, pathKey, rawPath))
                return {ExitCode::BackError, ExitCause::MissingReplyData};

            if (!Utility::normalizedSyncName(rawPath, path)) {
                LOGW_DEBUG(Log::instance()->getLogger(),
                           L"Error in Utility::normalizedSyncName: " << Utility::formatSyncName(rawPath));
                // Ignore the item
                continue;
            }
        }

        SyncTime modifiedTime = 0;
        const bool mandatory = false;
        if (!JsonParserUtility::extractValue(obj, lastModifiedAtKey, modifiedTime, mandatory))
            return {ExitCode::BackError, ExitCause::MissingReplyData};

        bool accessDenied = false;
        if (auto capabilitiesObj = obj->getObject(capabilitiesKey); capabilitiesObj) {
            bool canShow = true;
            if (!JsonParserUtility::extractValue(capabilitiesObj, canShowKey, canShow))
                return {ExitCode::BackError, ExitCause::MissingReplyData};

            accessDenied = !canShow;
        }

        RemoteNodeId parentId;
        if (!JsonParserUtility::extractValue(obj, parentIdKey, parentId))
            return {ExitCode::BackError, ExitCause::MissingReplyData};

        NodeInfo nodeInfo(QString::fromStdString(nodeId), SyncName2QStr(name),
                          -1, // Size is not set as it can be long to calculate.
                          parentId.c_str(), modifiedTime, SyncName2QStr(path));
        nodeInfo.setAccessDenied(accessDenied);

        (void) _remoteNodeInfoList.emplace_back(std::move(nodeInfo));
    }

    return ExitCode::Ok;
}

ExitInfo GetFilesInDirectoryJob::v2RemoteNodeInfoList(RemoteNodeInfoList &remoteNodeInfoList) const {
    // Data is already deserialized by handleResponse().
    remoteNodeInfoList = _remoteNodeInfoList;
    return ApiTranslator::translateV3ToV2(driveDbId(), remoteNodeInfoList);
}


ExitInfo GetFilesInDirectoryJob::handleResponse(std::istream &is) {
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
