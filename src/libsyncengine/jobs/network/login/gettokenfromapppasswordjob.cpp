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

#include "gettokenfromapppasswordjob.h"
#include "config.h"
#include "jobs/network/networkjobsparams.h"

namespace KDC {

GetTokenFromAppPasswordJob::GetTokenFromAppPasswordJob(const std::string &username, const std::string &password) :
    AbstractLoginJob(), _username(username), _password(password) {}

ExitInfo GetTokenFromAppPasswordJob::setData() {
    Poco::URI uri;
    uri.addQueryParameter(usernameKey, _username);
    uri.addQueryParameter(passwordKey, _password);
    uri.addQueryParameter(grantTypeKey, grantTypePassword);
    uri.addQueryParameter(clientIdKey, CLIENT_ID);

    _data = uri.getRawQuery();
    return ExitCode::Ok;
}

} // namespace KDC
