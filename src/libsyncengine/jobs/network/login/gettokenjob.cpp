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

#include "gettokenjob.h"
#include "config.h"
#include "jobs/network/networkjobsparams.h"
#include "libcommonserver/utility/utility.h"

namespace KDC {

GetTokenJob::GetTokenJob(const std::string &authorizationCode, const std::string &codeVerifier) :
    AbstractLoginJob(),
    _authorizationCode(authorizationCode),
    _codeVerifier(codeVerifier) {
#if defined(KD_MACOS)
    if (!Utility::preventSleeping(true)) {
        LOG_WARN(_logger, "Error in Utility::preventSleeping");
    }
#endif
}

GetTokenJob::~GetTokenJob() {
#if defined(KD_MACOS)
    if (!Utility::preventSleeping(false)) {
        LOG_WARN(_logger, "Error in Utility::preventSleeping");
    }
#endif
}

ExitInfo GetTokenJob::setData() {
    Poco::URI uri;
    uri.addQueryParameter(grantTypeKey, grantTypeAuthorization);
    uri.addQueryParameter(codeKey, _authorizationCode);
    uri.addQueryParameter(codeVerifierKey, _codeVerifier);
    uri.addQueryParameter(clientIdKey, CLIENT_ID);
    uri.addQueryParameter(redirectUriKey, REDIRECT_URI);

    _data = uri.getRawQuery();
    return ExitCode::Ok;
}

} // namespace KDC
