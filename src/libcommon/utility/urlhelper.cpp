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

#include "urlhelper.h"

#include "utility.h"

namespace KDC {

static const std::string prodInfomaniakApiUrl = "https://api.infomaniak.com/";
static const std::string preprodInfomaniakApiUrl = "https://api.preprod.dev.infomaniak.ch/";
static const std::string prodKDriveApiUrl = "https://api.kdrive.infomaniak.com/";
static const std::string preprodKDriveApiUrl = "https://api.kdrive.preprod.dev.infomaniak.ch/";
static const std::string prodNotifyApiUrl = "https://notify.kdrive.infomaniak.com/";
static const std::string preprodNotifyApiUrl = "https://notify.kdrive.preprod.dev.infomaniak.ch/";
static const std::string prodLoginApiUrl = "https://login.infomaniak.com";
static const std::string preprodLoginApiUrl = "https://login.preprod.dev.infomaniak.ch";

std::string UrlHelper::infomaniakApiUrl(const uint8_t version /*= 2*/, const bool forceProd /*= false*/) {
    return (usePreProdUrl() && !forceProd ? preprodInfomaniakApiUrl : prodInfomaniakApiUrl) + std::to_string(version);
}

std::string UrlHelper::kDriveApiUrl(const uint8_t version /*= 2*/) {
    return (usePreProdUrl() ? preprodKDriveApiUrl : prodKDriveApiUrl) + std::to_string(version);
}

std::string UrlHelper::notifyApiUrl(const uint8_t version /*= 2*/) {
    return (usePreProdUrl() ? preprodNotifyApiUrl : prodNotifyApiUrl) + std::to_string(version);
}

std::string UrlHelper::loginApiUrl() {
    return usePreProdUrl() ? preprodLoginApiUrl : prodLoginApiUrl;
}

bool UrlHelper::usePreProdUrl() {
    static const bool usePreProdUrl = CommonUtility::envVarValue("KDRIVE_USE_PREPROD_URL") == "1";
    return usePreProdUrl;
}

} // namespace KDC
