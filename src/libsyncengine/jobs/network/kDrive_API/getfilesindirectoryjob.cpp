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

#include "jobs/network/kDrive_API/getfilesindirectoryjob.h"
#include "jobs/network/apitranslator.h"

#include "utility/jsonparserutility.h"


#include <Poco/Net/HTTPRequest.h>

namespace KDC {

GetFilesInDirectoryJob::GetFilesInDirectoryJob(const int userDbId, const int driveId, NodeId fileId, std::string cursorInput,
                                               const bool dirOnly /*= false*/, const bool translateV2ToV3 /*= false */) :
    AbstractTokenNetworkJob(ApiType::Drive, userDbId, 0, 0, driveId),
    _fileId(std::move(fileId)),
    _cursorInput(std::move(cursorInput)),
    _dirOnly(dirOnly) {
    _apiVersion = 3;
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
    if (translateV2ToV3) ApiTranslator::translateV2ToV3(driveDbId(), _fileId);
}

GetFilesInDirectoryJob::GetFilesInDirectoryJob(const int driveDbId, NodeId fileId, std::string cursorInput,
                                               const bool dirOnly /*= false*/, const bool translateV2ToV3 /*= false */) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0),
    _fileId(std::move(fileId)),
    _cursorInput(std::move(cursorInput)),
    _dirOnly(dirOnly) {
    _apiVersion = 3;
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
    if (translateV2ToV3) ApiTranslator::translateV2ToV3(driveDbId, _fileId);
}

std::string GetFilesInDirectoryJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/";
    str += _fileId;
    str += "/files";

    return str;
}

void GetFilesInDirectoryJob::setQueryParameters(Poco::URI &uri) {
    if (_dirOnly) {
        uri.addQueryParameter("type[]", "dir");
    }
    if (_withPath) {
        uri.addQueryParameter("with", "path,capabilities");
    } else {
        uri.addQueryParameter("with", "capabilities");
    }

    if (!_cursorInput.empty()) {
        uri.addQueryParameter("cursor", _cursorInput);
    }

    uri.addQueryParameter("limit", std::to_string(_limit));
}

ExitInfo GetFilesInDirectoryJob::deserializeDataArray() {
    const auto dataArray = jsonRes()->getArray(dataKey);
    if (!dataArray) {
        LOG_WARN(_logger, "Missing data array for files in directory");
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    }

    for (auto it = dataArray->begin(); it != dataArray->end(); ++it) {
        const auto obj = it->extract<Poco::JSON::Object::Ptr>();

        NodeId nodeId;
        if (!JsonParserUtility::extractValue(obj, idKey, nodeId)) {
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }

        SyncName rawName;
        if (!JsonParserUtility::extractValue(obj, nameKey, rawName)) {
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }

        SyncName name;
        if (!Utility::normalizedSyncName(rawName, name)) {
            LOGW_DEBUG(Log::instance()->getLogger(),
                       L"Error in Utility::normalizedSyncName: " << Utility::formatSyncName(rawName));
            // Ignore the item
            continue;
        }

        std::string path;
        if (_withPath) {
            std::string rawPath;
            if (!JsonParserUtility::extractValue(obj, pathKey, rawPath)) {
                return {ExitCode::BackError, ExitCause::MissingReplyData};
            }

            if (!Utility::normalizedSyncName(rawPath, path)) {
                LOGW_DEBUG(Log::instance()->getLogger(),
                           L"Error in Utility::normalizedSyncName: " << Utility::formatSyncName(rawPath));
                // Ignore the item
                continue;
            }
        }

        SyncTime modifiedTime = 0;
        if (!JsonParserUtility::extractValue(obj, lastModifiedAtKey, modifiedTime, false)) {
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }

        Poco::JSON::Object::Ptr capabilitiesObj = obj->getObject(capabilitiesKey);
        bool accessDenied = false;
        if (capabilitiesObj) {
            bool canShow = true;
            if (!JsonParserUtility::extractValue(capabilitiesObj, canShowKey, canShow)) {
                return {ExitCode::BackError, ExitCause::MissingReplyData};
            }
            accessDenied = !canShow;
        }

        std::string parentId;
        if (!JsonParserUtility::extractValue(obj, parentIdKey, parentId)) {
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }

        NodeInfo nodeInfo(QString::fromStdString(nodeId), SyncName2QStr(name),
                          -1, // Size is not set as it can be long to calculate.
                          parentId.c_str(), modifiedTime, SyncName2QStr(path));
        nodeInfo.setAccessDenied(accessDenied);

        _nodeInfoList.emplace_back(std::move(nodeInfo));
    }

    return ExitCode::Ok;
}

ExitInfo GetFilesInDirectoryJob::handleResponse(std::istream &is) {
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

    return deserializeDataArray();
}

} // namespace KDC
