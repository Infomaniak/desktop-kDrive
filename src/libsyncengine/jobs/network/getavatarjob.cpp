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

#include "getavatarjob.h"
#include "jobs/network/networkjobsparams.h"
#include "libcommonserver/log/log.h"

#include <Poco/DOM/DOMParser.h>
#include <Poco/DOM/Document.h>
#include <Poco/DOM/AutoPtr.h>
#include <Poco/Net/HTTPRequest.h>

namespace KDC {

GetAvatarJob::GetAvatarJob(std::string url) :
    _avatarUrl(url),
    _avatar(nullptr) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
}

std::string GetAvatarJob::getUrl() {
    return _avatarUrl;
}

std::string GetAvatarJob::getContentType(bool &canceled) {
    canceled = false;
    return std::string();
}

bool GetAvatarJob::handleError(const std::string &replyBody, const Poco::URI &uri) {
    Poco::XML::DOMParser parser;
    Poco::AutoPtr<Poco::XML::Document> pDoc = parser.parseString(replyBody);
    Poco::XML::Node *pNode = pDoc->getNodeByPath(errorCodePathKey);
    if (pNode != nullptr) {
        _errorCode = pNode->innerText();
        LOG_WARN(_logger, "Error in request: " << uri.toString() << ". Error code: " << _errorCode);
    } else {
        LOG_WARN(_logger, "Unknown error in request: " << uri.toString());
    }

    _exitInfo = {ExitCode::BackError, ExitCause::ApiErr};

    return false;
}

bool GetAvatarJob::handleResponse(std::istream &is) {
    _avatar = std::make_shared<std::vector<char>>(std::istreambuf_iterator(is), (std::istreambuf_iterator<char>()));
    return true;
}

} // namespace KDC
