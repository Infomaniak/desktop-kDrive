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

#include "postfilelinkjob.h"
#include "jobs/network/networkjobsparams.h"

#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>

namespace KDC {

PostFileLinkJob::PostFileLinkJob(const DriveDbId driveDbId, RemoteNodeId nodeId) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, driveDbId, 0),
    _nodeId(std::move(nodeId)) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_POST;
    _apiVersion = 2;
}

std::string PostFileLinkJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/";
    str += _nodeId;
    str += "/link";

    return str;
}

void KDC::PostFileLinkJob::setQueryParameters(Poco::URI &uri) {
    uri.addQueryParameter("right", "public");
}

// Refines the generic causes so that the GUI can explain why the link could not be created.
ExitInfo PostFileLinkJob::handleError(const std::string &replyBody, const Poco::URI &uri) {
    auto exitInfo = AbstractTokenNetworkJob::handleError(replyBody, uri);
    if (exitInfo.code() != ExitCode::BackError) return exitInfo;

    // 401 and 404 responses are fully handled by the base class, whatever the error code.
    if (const auto status = httpResponse().getStatus();
        status == Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED || status == Poco::Net::HTTPResponse::HTTP_NOT_FOUND) {
        return exitInfo;
    }

    switch (getNetworkErrorCode(backError().code())) {
        case NetworkErrorCode::LimitExceededError:
            exitInfo.setCause(ExitCause::ShareLinkLimitReached);
            break;
        case NetworkErrorCode::ForbiddenError:
        case NetworkErrorCode::PermissionDenied:
            exitInfo.setCause(ExitCause::HttpErrForbidden);
            break;
        default:
            break;
    }

    return exitInfo;
}

} // namespace KDC
