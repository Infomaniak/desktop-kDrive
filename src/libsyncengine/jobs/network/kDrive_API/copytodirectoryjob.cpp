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

#include "copytodirectoryjob.h"

#include "jobs/network/jobexceptions.h"
#include "jobs/network/kDrive_API/apitranslator.h"

#include "libcommonserver/utility/jsonparserutility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

CopyToDirectoryJob::CopyToDirectoryJob(const DriveDbId driveDbId, RemoteNodeId remoteFileId, RemoteNodeId remoteDestId,
                                       SyncName newName) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, driveDbId, 0),
    _remoteFileId(std::move(remoteFileId)),
    _remoteDestId(std::move(remoteDestId)),
    _newName(std::move(newName)) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_POST;

    if (const auto exitInfo = ApiTranslator::translateV2ToV3(userDbId(), driveId(), _remoteDestId); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ApiTranslator::translateV2ToV3: " << exitInfo);
        throw JobException("Translation error in UploadJob::UploadJob.");
    }
}

ExitInfo CopyToDirectoryJob::handleResponse(std::istream &is) {
    if (const auto exitInfo = AbstractTokenNetworkJob::handleResponse(is); !exitInfo) return exitInfo;

    if (!jsonRes()) return ExitCode::Ok;

    if (Poco::JSON::Object::Ptr dataObj = jsonRes()->getObject(dataKey); dataObj) {
        if (!JsonParserUtility::extractValue(dataObj, idKey, _nodeId)) {
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }

        if (!JsonParserUtility::extractValue(dataObj, lastModifiedAtKey, _modtime)) {
            return {ExitCode::BackError, ExitCause::MissingReplyData};
        }
    }

    return ExitCode::Ok;
}

std::string CopyToDirectoryJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/";
    str += _remoteFileId;
    str += "/copy/";
    str += _remoteDestId;

    return str;
}

ExitInfo CopyToDirectoryJob::setData() {
    Poco::JSON::Object json;
    json.set("name", _newName);

    std::stringstream ss;
    json.stringify(ss);
    _data = ss.str();

    return ExitCode::Ok;
}

} // namespace KDC
