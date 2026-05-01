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

#include "uploadsessioncanceljob.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

UploadSessionCancelJob::UploadSessionCancelJob(const UploadSessionType uploadType, const DriveDbId driveDbId,
                                               const SyncPath &filepath, const std::string &sessionToken) :
    AbstractUploadSessionJob(uploadType, driveDbId, filepath, SessionInfo{.url = "", .token = sessionToken}) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_DELETE;
}

UploadSessionCancelJob::UploadSessionCancelJob(const UploadSessionType uploadType, const std::string &sessionToken) :
    AbstractUploadSessionJob(uploadType, 0, "", SessionInfo{.url = "", .token = sessionToken}) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_DELETE;
}

std::string UploadSessionCancelJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/upload/session/";
    str += _sessionInfo.token;
    return str;
}

ExitInfo UploadSessionCancelJob::handleError(const std::string &replyBody, const Poco::URI &uri) {
    if (httpResponse().getStatus() == Poco::Net::HTTPResponse::HTTP_BAD_REQUEST) {
        return ExitCode::BackError;
    }

    return AbstractTokenNetworkJob::handleError(replyBody, uri);
}


} // namespace KDC
