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
    if (getStatusCode() == HTTPResponse::HTTP_OK) {
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

bool AbstractLoginJob::handleResponse(std::istream &is) {
    std::string str(std::istreambuf_iterator<char>(is), {});
    _apiToken = ApiToken(str);

    return true;
}

bool AbstractLoginJob::handleError(std::istream &is, const Poco::URI &uri) {
    Poco::JSON::Parser jsonParser;
    Poco::JSON::Object::Ptr jsonError;
    try {
        jsonError = jsonParser.parse(is).extract<Poco::JSON::Object::Ptr>();
    } catch (Poco::Exception &exc) {
        LOG_WARN(_logger, "Reply " << jobId() << " received doesn't contain a valid JSON error: " << exc.displayText().c_str());

        // Try to parse as string
        std::stringstream errorStream;
        errorStream << "Reply " << jobId();

        std::string str(std::istreambuf_iterator<char>(is), {});
        if (!str.empty()) {
            errorStream << ", error: " << str.c_str();
        }

        errorStream << ", content type: " << _resHttp.getContentType().c_str();
        errorStream << ", reason: " << _resHttp.getReason().c_str();

        std::string errorStr = errorStream.str();
#ifdef NDEBUG
        sentry_capture_event(sentry_value_new_message_event(SENTRY_LEVEL_WARNING, "Login error", errorStr.c_str()));
#endif
        LOG_WARN(_logger, "Login error: " << errorStr.c_str());

        _exitCode = ExitCodeBackError;
        _exitCause = ExitCauseApiErr;
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
        _exitCode = ExitCodeBackError;
    } else {
        JsonParserUtility::extractValue(jsonError, errorKey, _errorCode, false);

        std::string errorReason;
        JsonParserUtility::extractValue(jsonError, reasonKey, errorReason, false);

        if (_errorCode == invalidGrant || errorReason == refreshTokenRevoked) {
            _errorDescr = errorReason;
            LOG_WARN(_logger, "Error in request " << uri.toString().c_str() << " : refresh token has been revoked ");
            noRetry();
            _exitCode = ExitCodeInvalidToken;
        } else {
            LOG_WARN(_logger, "Error in request " << uri.toString().c_str() << " : " << errorReason.c_str());
            _exitCode = ExitCodeBackError;
        }
    }

    _exitCause = ExitCauseLoginError;
    return false;
}

}  // namespace KDC
