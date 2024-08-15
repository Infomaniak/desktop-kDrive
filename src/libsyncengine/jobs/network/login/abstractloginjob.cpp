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

#include "abstractloginjob.h"
#include "config.h"
#include "jobs/network/networkjobsparams.h"
#include "libcommonserver/utility/utility.h"
#include "requests/parameterscache.h"
#include "utility/jsonparserutility.h"

#include <Poco/JSON/Parser.h>

namespace KDC {

AbstractLoginJob::AbstractLoginJob() {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_POST;
}

bool AbstractLoginJob::hasErrorApi(std::string *errorCode, std::string *errorDescr) {
    if (getStatusCode() == Poco::Net::HTTPResponse::HTTP_OK) {
        return false;
    }

    if (errorCode) {
        *errorCode = _errorCode;
        if (errorDescr) {
            *errorDescr = _errorDescr;
        }
    }
    return true;
}

std::string AbstractLoginJob::getSpecificUrl() {
    return "/token";
}

std::string AbstractLoginJob::getUrl() {
    return std::string(LOGIN_URL) + getSpecificUrl();
}

std::string AbstractLoginJob::getContentType(bool &canceled) {
    canceled = false;
    return "application/x-www-form-urlencoded";
}

bool AbstractLoginJob::handleResponse(std::istream &inputStream) {
    std::string str(std::istreambuf_iterator<char>(inputStream), {});
    _apiToken = ApiToken(str);

    return true;
}

bool AbstractLoginJob::handleError(std::istream &inputStream, const Poco::URI &uri) {
    Poco::JSON::Parser jsonParser;
    Poco::JSON::Object::Ptr jsonError;
    try {
        jsonError = jsonParser.parse(inputStream).extract<Poco::JSON::Object::Ptr>();
    } catch (Poco::Exception &exc) {
        LOG_WARN(_logger, "Reply " << jobId() << " received doesn't contain a valid JSON error: " << exc.displayText().c_str());
        Utility::logGenericServerError(_logger, "Login error", inputStream, _resHttp);

        _exitCode = ExitCode::BackError;
        _exitCause = ExitCause::ApiErr;
        return false;
    }

    if (isExtendedLog()) {
        std::ostringstream os;
        jsonError->stringify(os);
        LOGW_DEBUG(_logger, L"Reply " << jobId() << L" received: " << Utility::s2ws(os.str()).c_str());
    }

    Poco::JSON::Object::Ptr errorObj = jsonError->getObject(errorKey);
    if (errorObj) {
        if (!JsonParserUtility::extractValue(errorObj, codeKey, _errorCode)) {
            return false;
        }
        if (!JsonParserUtility::extractValue(errorObj, descriptionKey, _errorDescr)) {
            return false;
        }
        LOG_WARN(_logger,
                 "Error in request " << uri.toString().c_str() << " : " << _errorCode.c_str() << " - " << _errorDescr.c_str());
        _exitCode = ExitCode::BackError;
    } else {
        JsonParserUtility::extractValue(jsonError, errorKey, _errorCode, false);

        std::string errorReason;
        JsonParserUtility::extractValue(jsonError, reasonKey, errorReason, false);

        if (getNetworkErrorCode(_errorCode) == NetworkErrorCode::invalidGrant ||
            getNetworkErrorReason(errorReason) == NetworkErrorReason::refreshTokenRevoked) {
            _errorDescr = errorReason;
            LOG_WARN(_logger, "Error in request " << uri.toString().c_str() << " : refresh token has been revoked ");
            noRetry();
            _exitCode = ExitCode::InvalidToken;
        } else {
            LOG_WARN(_logger, "Error in request " << uri.toString().c_str() << " : " << errorReason.c_str());
            _exitCode = ExitCode::BackError;
        }
    }

    _exitCause = ExitCause::LoginError;
    return false;
}

}  // namespace KDC
