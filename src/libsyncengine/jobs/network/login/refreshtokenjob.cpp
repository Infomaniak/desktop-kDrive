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

#include "refreshtokenjob.h"
#include "config.h"
#include "jobs/network/networkjobsparams.h"
#include "libcommonserver/utility/utility.h"

namespace KDC {

RefreshTokenJob::RefreshTokenJob(const ApiToken &apiToken) {
    _apiToken = apiToken;

#if defined(KD_MACOS)
    if (!Utility::preventSleeping(true)) {
        LOG_WARN(_logger, "Error in Utility::preventSleeping");
    }
#endif
}

RefreshTokenJob::~RefreshTokenJob() {
#if defined(KD_MACOS)
    if (!Utility::preventSleeping(false)) {
        LOG_WARN(_logger, "Error in Utility::preventSleeping");
    }
#endif
}

ExitInfo RefreshTokenJob::setData() {
    Poco::URI uri;
    uri.addQueryParameter(grantTypeKey, refreshTokenKey);
    uri.addQueryParameter(refreshTokenKey, _apiToken.refreshToken().c_str());
    uri.addQueryParameter(clientIdKey, CLIENT_ID);
    uri.addQueryParameter(durationKey, infiniteKey);

    _data = uri.getRawQuery();
    return ExitCode::Ok;
}

} // namespace KDC
