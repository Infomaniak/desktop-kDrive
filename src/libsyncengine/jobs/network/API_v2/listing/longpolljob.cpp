// Infomaniak kDrive - Desktop
// Copyright (C) 2023-2025 Infomaniak Network SA
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "longpolljob.h"

namespace KDC {

#define API_TIMEOUT 50

LongPollJob::LongPollJob(const int driveDbId, const std::string &cursor, std::unordered_set<NodeId> blacklist /*= {}*/) :
    AbstractListingJob(driveDbId, blacklist), _cursor(cursor) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
    _customTimeout = API_TIMEOUT + 5; // Must be < 1 min (VPNs' default timeout)
}

std::string LongPollJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/listing/longpoll";
    return str;
}

void LongPollJob::setSpecificQueryParameters(Poco::URI &uri) {
    uri.addQueryParameter("cursor", _cursor);
    uri.addQueryParameter("timeout", std::to_string(API_TIMEOUT) + "s");
}

} // namespace KDC
