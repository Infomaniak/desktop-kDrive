/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#include "apitoken.h"
#include "utility/jsonparserutility.h"

#include <Poco/JSON/Parser.h>

namespace KDC {

const std::string accessTokenKey = "access_token";
const std::string refreshTokenKey = "refresh_token";
const std::string tokenTypeKey = "token_type";
const std::string expiresInKey = "expires_in";
const std::string userIdKey = "user_id";
const std::string scopeKey = "scope";

ApiToken::ApiToken() {}

ApiToken::ApiToken(const std::string &data) : _rawData(data) {
    Poco::JSON::Parser parser;
    Poco::Dynamic::Var res;
    try {
        res = parser.parse(data);
    } catch (Poco::Exception &) {
        return;
    }

    Poco::JSON::Object::Ptr jsonObj = res.extract<Poco::JSON::Object::Ptr>();
    if (!JsonParserUtility::extractValue(jsonObj, accessTokenKey, _accessToken)) {
        return;
    }
    if (!JsonParserUtility::extractValue(jsonObj, refreshTokenKey, _refreshToken, false)) {
        return;
    }
    if (!JsonParserUtility::extractValue(jsonObj, tokenTypeKey, _tokenType, false)) {
        return;
    }
    if (!JsonParserUtility::extractValue(jsonObj, expiresInKey, _expiresIn, false)) {
        return;
    }
    if (!JsonParserUtility::extractValue(jsonObj, userIdKey, _userId)) {
        return;
    }
    if (!JsonParserUtility::extractValue(jsonObj, scopeKey, _scope, false)) {
        return;
    }
}

std::string ApiToken::reconstructJsonString() const {
    Poco::JSON::Object obj;
    obj.set(accessTokenKey, _accessToken);
    obj.set(refreshTokenKey, _refreshToken);
    obj.set(tokenTypeKey, _tokenType);
    obj.set(expiresInKey, _expiresIn);
    obj.set(userIdKey, _userId);
    obj.set(scopeKey, _scope);

    std::ostringstream out;
    obj.stringify(out);
    return out.str();
}
bool ApiToken::operator==(const ApiToken &other) const {
    return this->_accessToken == other._accessToken && this->_refreshToken == other._refreshToken &&
           this->_tokenType == other._tokenType && this->_expiresIn == other._expiresIn && this->_userId == other._userId &&
           this->_scope == other._scope;
}

} // namespace KDC
