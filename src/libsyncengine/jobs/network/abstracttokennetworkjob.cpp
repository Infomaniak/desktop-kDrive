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

#include "abstracttokennetworkjob.h"
#include "config.h"
#include "jobs/network/networkjobsparams.h"
#include "libcommonserver/utility/utility.h"
#include "libparms/db/parmsdb.h"
#include "keychainmanager/keychainmanager.h"
#include "utility/jsonparserutility.h"

#include <unordered_map>

#include <log4cplus/loggingmacros.h>

#include <Poco/JSON/Parser.h>

constexpr char API_PREFIX_DRIVE[] = "/drive";
constexpr char API_PREFIX_DESKTOP[] = "/desktop";
constexpr char API_PREFIX_PROFILE[] = "/profile";
constexpr char API_PREFIX_APP_INFO[] = "/app-information";

constexpr char ABSTRACTTOKENNETWORKJOB_NEW_ERROR_MSG[] = "Failed to create AbstractTokenNetworkJob instance!";
constexpr char ABSTRACTTOKENNETWORKJOB_NEW_ERROR_MSG_INVALID_TOKEN[] = "Invalid Token";
constexpr char ABSTRACTTOKENNETWORKJOB_EXEC_ERROR_MSG[] = "Failed to execute AbstractTokenNetworkJob!";

constexpr int TOKEN_LIFETIME = 7200; // 2 hours

namespace KDC {
std::unordered_map<int, std::pair<std::shared_ptr<Login>, int> > AbstractTokenNetworkJob::_userToApiKeyMap;
std::unordered_map<int, std::pair<int, int> > AbstractTokenNetworkJob::_driveToApiKeyMap;

AbstractTokenNetworkJob::AbstractTokenNetworkJob(ApiType apiType, int userDbId, int userId, int driveDbId, int driveId,
                                                 bool returnJson /*= true*/)
    : _apiType(apiType), _userDbId(userDbId), _userId(userId), _driveDbId(driveDbId), _driveId(driveId),
      _returnJson(returnJson) {
    if (!ParmsDb::instance()) {
        LOG_WARN(_logger, "ParmsDb must be initialized!");
        throw std::runtime_error(ABSTRACTTOKENNETWORKJOB_NEW_ERROR_MSG);
    }

    if (((_apiType == ApiDrive || _apiType == ApiNotifyDrive) && _driveDbId == 0 && (_userDbId == 0 || _driveId == 0))
        || ((_apiType == ApiProfile || _apiType == ApiDriveByUser) && _userDbId == 0)) {
        LOG_WARN(_logger, "Invalid parameters!");
        throw std::runtime_error(ABSTRACTTOKENNETWORKJOB_NEW_ERROR_MSG);
    }

    _token = loadToken();

    addRawHeader("Authorization", "Bearer " + _token);
}

AbstractTokenNetworkJob::AbstractTokenNetworkJob(ApiType apiType, bool returnJson /*= true*/)
    : AbstractTokenNetworkJob(apiType, 0, 0, 0, 0, returnJson) {
}

bool AbstractTokenNetworkJob::hasErrorApi(std::string *errorCode, std::string *errorDescr) {
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

ExitCause AbstractTokenNetworkJob::getExitCause() {
    if (_exitCause == ExitCauseUnknown) {
        if (!_errorCode.empty()) {
            return ExitCauseApiErr;
        } else {
            if (getStatusCode() == Poco::Net::HTTPResponse::HTTP_FORBIDDEN) {
                return ExitCauseHttpErrForbidden;
            } else {
                return ExitCauseHttpErr;
            }
        }
    } else {
        return _exitCause;
    }
}

void AbstractTokenNetworkJob::updateLoginByUserDbId(const Login &login, int userDbId) {
    auto it = _userToApiKeyMap.find(userDbId);
    if (it != _userToApiKeyMap.end()) {
        std::shared_ptr<Login> currentLogin = it->second.first;
        // get new credentials
        ApiToken newApiToken = login.apiToken();
        std::string newKeychainKey = login.keychainKey();
        // set new credentials to Login class
        currentLogin->setApiToken(newApiToken);
        currentLogin->setKeychainKey(newKeychainKey);
    }
}

std::string AbstractTokenNetworkJob::getSpecificUrl() {
    std::string str;
    switch (_apiType) {
        case ApiDrive:
        case ApiNotifyDrive:
            str += API_PREFIX_DRIVE;
            if (_driveId) {
                str += "/";
                str += std::to_string(_driveId);
            }
            break;
        case ApiDriveByUser:
            str += API_PREFIX_DRIVE;
            break;
        case ApiProfile:
            str += API_PREFIX_PROFILE;
            break;
        case ApiInfomaniak:
            str += API_PREFIX_APP_INFO;
            break;
        case ApiDesktop:
            str += API_PREFIX_DESKTOP;
            break;
    }

    return str;
}

bool AbstractTokenNetworkJob::handleUnauthorizedResponse() {
    // There is no longer any refresh of the token since v3.5.6
    // This code is only used when updating from a version < v3.5.6
    _exitCode = ExitCodeInvalidToken;
    if (std::string token = loadToken(); token != _token) {
        LOG_DEBUG(_logger, "Token refreshed by another request");
        _accessTokenAlreadyRefreshed = false;
        _token = token;
        addRawHeader("Authorization", "Bearer " + _token);
        _exitCode = ExitCodeTokenRefreshed;

        return true;
    }

    if (!_accessTokenAlreadyRefreshed || tokenUpdateDurationFromNow() > TOKEN_LIFETIME) {
        // The token has not already been refreshed or was refreshed more than its lifetime ago
        if (!refreshToken()) {
            LOG_WARN(_logger, "Refresh token failed");
            noRetry();

            return false;
        }

        LOG_DEBUG(_logger, "Refresh token succeeded");
        _exitCode = ExitCodeTokenRefreshed;

        return true;
    }

    LOG_WARN(_logger, "Token already refreshed once");

    return false;
}

bool AbstractTokenNetworkJob::defaultBackErrorHandling(NetworkErrorCode errorCode, const Poco::URI &uri) {
    static const std::map<NetworkErrorCode, AbstractTokenNetworkJob::ExitHandler> errorCodeHandlingMap = {
        {NetworkErrorCode::validationFailed, ExitHandler{ExitCauseInvalidName, "Invalid file or directory name"}},
        {
            NetworkErrorCode::uploadNotTerminatedError,
            ExitHandler{ExitCauseUploadNotTerminated, "Upload not terminated"}
        },
        {NetworkErrorCode::uploadError, ExitHandler{ExitCauseApiErr, "Upload failed"}},
        {NetworkErrorCode::destinationAlreadyExists, ExitHandler{ExitCauseFileAlreadyExist, "Operation refused"}},
        {NetworkErrorCode::conflictError, ExitHandler{ExitCauseFileAlreadyExist, "Operation refused"}},
        {NetworkErrorCode::accessDenied, ExitHandler{ExitCauseHttpErrForbidden, "Access denied"}},
        {NetworkErrorCode::fileTooBigError, ExitHandler{ExitCauseFileTooBig, "File too big"}},
        {NetworkErrorCode::quotaExceededError, ExitHandler{ExitCauseQuotaExceeded, "Quota exceeded"}}
    };

    const auto &errorHandling = errorCodeHandlingMap.find(errorCode);
    if (errorHandling == errorCodeHandlingMap.cend()) {
        LOG_WARN(_logger, "Error in request " << Utility::formatRequest(uri, _errorCode, _errorDescr).c_str());
        _exitCause = ExitCauseHttpErr;

        return false;
    }
    // Regular handling
    const auto &exitHandler = errorHandling->second;
    LOG_DEBUG(_logger, exitHandler.debugMessage.c_str());
    _exitCause = exitHandler.exitCause;

    return true;
}


bool AbstractTokenNetworkJob::handleError(std::istream &is, const Poco::URI &uri) {
    switch (_resHttp.getStatus()) {
        case Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED:
            return handleUnauthorizedResponse();
        case Poco::Net::HTTPResponse::HTTP_NOT_FOUND: {
            _exitCode = ExitCodeBackError;
            _exitCause = ExitCauseNotFound;
            return false;
        }
        default:
            break;
    }

    // Manage Content-Encoding
    std::string encoding;
    try {
        encoding = _resHttp.get("Content-Encoding");
    } catch (...) {
        // No Content-Encoding
    }

    std::stringstream ss;
    if (encoding == "gzip") {
        unzip(is, ss);
    } else {
        ss << is.rdbuf();
    }

    _exitCode = ExitCodeBackError;
    try {
        _error = Poco::JSON::Parser{}.parse(ss.str()).extract<Poco::JSON::Object::Ptr>();
    } catch (const Poco::Exception &exc) {
        LOGW_WARN(_logger, L"Reply " << jobId() << L" received doesn't contain a valid JSON error: "
                  << Utility::s2ws(exc.displayText()).c_str());
        Utility::logGenericServerError(_logger, "Request error", ss, _resHttp);
        _exitCause = ExitCauseApiErr;

        return false;
    }

    if (isExtendedLog()) {
        std::ostringstream os;
        _error->stringify(os);
        LOGW_DEBUG(_logger, L"Reply " << jobId() << L" received: " << Utility::s2ws(os.str()).c_str());
    }

    Poco::JSON::Object::Ptr errorObj = _error->getObject(errorKey);
    if (!JsonParserUtility::extractValue(errorObj, codeKey, _errorCode)) {
        return false;
    }
    if (!JsonParserUtility::extractValue(errorObj, descriptionKey, _errorDescr)) {
        return false;
    }

    const NetworkErrorCode errorCode = getNetworkErrorCode(_errorCode);
    switch (errorCode) {
        case KDC::NetworkErrorCode::notAuthorized: {
            if (!_accessTokenAlreadyRefreshed) {
                LOG_DEBUG(_logger, "Request failed: " << Utility::formatRequest(uri, _errorCode, _errorDescr).c_str()
                          << ". Refreshing access token.");

                if (!refreshToken()) {
                    LOG_WARN(_logger, "Refresh token failed");
                    _exitCode = ExitCodeInvalidToken;
                    return false;
                }

                LOG_DEBUG(_logger, "Refresh token succeeded");
                _exitCode = ExitCodeTokenRefreshed;
                return true;
            }
        }

        case KDC::NetworkErrorCode::productMaintenance:
        case KDC::NetworkErrorCode::driveIsInMaintenanceError: {
            LOG_DEBUG(_logger, "Product in maintenance");
            noRetry();
            if (auto contextObj = errorObj->getObject(contextKey); contextObj != nullptr) {
                std::string context;
                JsonParserUtility::extractValue(contextObj, reasonKey, context, false);
                if (getNetworkErrorReason(context) == NetworkErrorReason::notRenew) {
                    _exitCause = ExitCauseDriveNotRenew;
                    return false;
                }
            }
            _exitCause = ExitCauseDriveMaintenance;

            return false;
        }
        default:
            return defaultBackErrorHandling(errorCode, uri);
    }
}

std::string AbstractTokenNetworkJob::getUrl() {
    std::string apiUrl;
    switch (_apiType) {
        case ApiDrive:
        case ApiDriveByUser:
        case ApiDesktop:
            apiUrl = KDRIVE_API_V2_URL;
            break;
        case ApiNotifyDrive:
            apiUrl = NOTIFY_KDRIVE_V2_URL;
            break;
        case ApiProfile:
            apiUrl = GLOBAL_API_V2_URL;
            break;
        case ApiInfomaniak:
            apiUrl = INFOMANIAK_API_URL;
    }
    return apiUrl + getSpecificUrl();
}

bool AbstractTokenNetworkJob::handleResponse(std::istream &is) {
    if (_returnJson) return handleJsonResponse(is);
    return handleOctetStreamResponse(is);
}

bool AbstractTokenNetworkJob::handleJsonResponse(std::istream &is) {
    // Extract JSON
    try {
        _jsonRes = Poco::JSON::Parser().parse(is).extract<Poco::JSON::Object::Ptr>();
    } catch (Poco::Exception &exc) {
        LOG_DEBUG(_logger,
                  "Reply " << jobId() << " received doesn't contain a valid JSON payload: " << exc.displayText().c_str(
                  ));
        _exitCode = ExitCodeBackError;
        _exitCause = ExitCauseApiErr;
        return false;
    }

    // Extract reply
    std::ostringstream os;
    _jsonRes->stringify(os);
    if (isExtendedLog()) {
        LOGW_DEBUG(_logger, L"Reply " << jobId() << L" received: " << Utility::s2ws(os.str()).c_str());
    }

    // Check for maintenance error
    if (_jsonRes) {
        if (Poco::JSON::Object::Ptr dataObj = _jsonRes->getObject(dataKey); dataObj != nullptr) {
            std::string maintenanceReason;
            if (!JsonParserUtility::extractValue(dataObj, maintenanceReasonKey, maintenanceReason, false)) {
                return false;
            }

            if (getNetworkErrorReason(maintenanceReason) == NetworkErrorReason::notRenew) {
                noRetry();
                _exitCode = ExitCodeBackError;
                _exitCause = ExitCauseDriveNotRenew;
                return false;
            }
        }
    }

    return true;
}

bool AbstractTokenNetworkJob::handleJsonResponse(std::string &str) {
    // Extract JSON
    Poco::JSON::Parser jsonParser;
    try {
        _jsonRes = jsonParser.parse(str).extract<Poco::JSON::Object::Ptr>();
    } catch (Poco::Exception &exc) {
        LOG_DEBUG(_logger,
                  "Reply " << jobId() << " received doesn't contain a valid JSON error: " << exc.displayText().c_str());

        _exitCode = ExitCodeBackError;
        _exitCause = ExitCauseApiErr;
        return false;
    }

    // Extract reply
    if (isExtendedLog()) {
        LOGW_DEBUG(_logger, L"Reply " << jobId() << L" received: " << Utility::s2ws(str).c_str());
    }

    return true;
}

bool AbstractTokenNetworkJob::handleOctetStreamResponse(std::istream &is) {
    getStringFromStream(is, _octetStreamRes);

    return true;
}

std::string AbstractTokenNetworkJob::loadToken() {
    std::string token;
    if (_apiType == ApiDesktop) {
        // Fetch the drive identifier of the first available sync.
        std::vector<Sync> syncList;
        if (!ParmsDb::instance()->selectAllSyncs(syncList)) {
            LOG_WARN(_logger, "Error in ParmsDb::selectAllSyncs");
            throw std::runtime_error(ABSTRACTTOKENNETWORKJOB_NEW_ERROR_MSG);
        }

        if (syncList.empty()) {
            LOG_WARN(_logger, "No sync found");
            throw std::runtime_error(ABSTRACTTOKENNETWORKJOB_NEW_ERROR_MSG);
        }

        _driveDbId = syncList[0].driveDbId();
    }

    switch (_apiType) {
        case ApiDrive:
        case ApiDesktop:
        case ApiNotifyDrive: {
            if (_driveDbId) {
                auto it = _driveToApiKeyMap.find(_driveDbId);
                if (it != _driveToApiKeyMap.end()) {
                    // driveDbId found in Drive cache
                    _userDbId = it->second.first;
                    _driveId = it->second.second;
                } else {
                    // Get drive
                    Drive drive;
                    bool found;
                    if (!ParmsDb::instance()->selectDrive(_driveDbId, drive, found)) {
                        LOG_WARN(_logger, "Error in ParmsDb::selectDrive");
                        throw std::runtime_error(ABSTRACTTOKENNETWORKJOB_NEW_ERROR_MSG);
                    }
                    if (!found) {
                        LOG_WARN(_logger, "Drive not found for driveDbId=" << _driveDbId);
                        throw std::runtime_error(ABSTRACTTOKENNETWORKJOB_NEW_ERROR_MSG);
                    }

                    _driveId = drive.driveId();

                    // Get account
                    Account account;
                    if (!ParmsDb::instance()->selectAccount(drive.accountDbId(), account, found)) {
                        LOG_WARN(_logger, "Error in ParmsDb::selectAccount");
                        throw std::runtime_error(ABSTRACTTOKENNETWORKJOB_NEW_ERROR_MSG);
                    }
                    if (!found) {
                        LOG_WARN(_logger, "Account not found for accountDbId=" << drive.accountDbId());
                        throw std::runtime_error(ABSTRACTTOKENNETWORKJOB_NEW_ERROR_MSG);
                    }

                    _userDbId = account.userDbId();

                    if (_userToApiKeyMap.find(_userDbId) == _userToApiKeyMap.end()) {
                        // Get user
                        User user;
                        if (!ParmsDb::instance()->selectUser(_userDbId, user, found)) {
                            LOG_WARN(_logger, "Error in ParmsDb::selectUser");
                            throw std::runtime_error(ABSTRACTTOKENNETWORKJOB_NEW_ERROR_MSG);
                        }
                        if (!found) {
                            LOG_WARN(_logger, "User not found for userDbId=" << _userDbId);
                            throw std::runtime_error(ABSTRACTTOKENNETWORKJOB_NEW_ERROR_MSG);
                        }

                        // Read token form keystore
                        std::shared_ptr<Login> login = std::shared_ptr<Login>(new Login(user.keychainKey()));
                        if (!login->hasToken()) {
                            LOG_WARN(_logger, "Failed to retrieve access token");
                            _exitCode = ExitCodeInvalidToken;
                            throw std::runtime_error(ABSTRACTTOKENNETWORKJOB_NEW_ERROR_MSG_INVALID_TOKEN);
                        }

                        _userToApiKeyMap[_userDbId] = {login, user.userId()};
                    }

                    _driveToApiKeyMap[_driveDbId] = {_userDbId, _driveId};
                }
            }

            auto it = _userToApiKeyMap.find(_userDbId);
            if (it != _userToApiKeyMap.end()) {
                // userDbId found in User cache
                _userId = it->second.second;
                token = it->second.first->apiToken().accessToken();
            } else {
                LOG_WARN(_logger, "User cache not set for userDbId=" << _userDbId);
                throw std::runtime_error(ABSTRACTTOKENNETWORKJOB_NEW_ERROR_MSG);
            }
            break;
        }
        case ApiProfile:
        case ApiDriveByUser: {
            auto it = _userToApiKeyMap.find(_userDbId);
            if (it != _userToApiKeyMap.end()) {
                // userDbId found in User cache
                _userId = it->second.second;
                token = it->second.first->apiToken().accessToken();
            } else {
                // Get user
                User user;
                bool found = false;
                if (!ParmsDb::instance()->selectUser(_userDbId, user, found)) {
                    LOG_WARN(_logger, "Error in ParmsDb::selectUser");
                    throw std::runtime_error(ABSTRACTTOKENNETWORKJOB_NEW_ERROR_MSG);
                }
                if (!found) {
                    LOG_WARN(_logger, "User not found for userDbId=" << _userDbId);
                    throw std::runtime_error(ABSTRACTTOKENNETWORKJOB_NEW_ERROR_MSG);
                }

                // Read token form keystore
                std::shared_ptr<Login> login = std::shared_ptr<Login>(new Login(user.keychainKey()));
                if (!login->hasToken()) {
                    LOG_WARN(_logger, "Failed to retrieve access token");
                    _exitCode = ExitCodeInvalidToken;
                    throw std::runtime_error(ABSTRACTTOKENNETWORKJOB_NEW_ERROR_MSG_INVALID_TOKEN);
                }

                _userId = user.userId();
                token = login->apiToken().accessToken();
                _userToApiKeyMap[_userDbId] = {login, _userId};
            }
            break;
        }
        default:
            // No token required
            break;
    }

    return token;
}

std::string AbstractTokenNetworkJob::getContentType(bool &canceled) {
    canceled = false;
    return mimeTypeJson;
}

bool AbstractTokenNetworkJob::refreshToken() {
    _accessTokenAlreadyRefreshed = true;

    auto it = _userToApiKeyMap.find(_userDbId);
    if (it != _userToApiKeyMap.end()) {
        // userDbId found in User cache
        std::shared_ptr<Login> login = it->second.first;
        ExitCode exitCode = login->refreshToken();
        if (exitCode != ExitCodeOk) {
            LOG_WARN(_logger, "Failed to refresh token : " << exitCode << " - " << login->error().c_str() << " - "
                     << login->errorDescr().c_str());
            _exitCause = ExitCauseLoginError;
            _exitCode = exitCode;

            // Clear the keychain key
            User user;
            bool found = false;
            if (!ParmsDb::instance()->selectUser(_userDbId, user, found)) {
                LOG_WARN(_logger, "Error in ParmsDb::selectUser");
                return false;
            }
            if (!found) {
                LOG_WARN(_logger, "User not found for userDbId=" << _userDbId);
                return false;
            }

            KeyChainManager::instance()->deleteToken(user.keychainKey());

            user.setKeychainKey(""); // Clear the keychainKey
            ParmsDb::instance()->updateUser(user, found);
            if (!found) {
                LOG_WARN(_logger, "User not found for userDbId=" << _userDbId);
                return false;
            }

            return false;
        }

        _token = login->apiToken().accessToken();
        addRawHeader("Authorization", "Bearer " + _token);
        return true;
    } else {
        LOG_WARN(_logger, "User cache not set for userDbId=" << _userDbId);
        _exitCause = ExitCauseLoginError;
        _exitCode = ExitCodeDataError;
        return false;
    }

    return true;
}

long AbstractTokenNetworkJob::tokenUpdateDurationFromNow() {
    auto it = _userToApiKeyMap.find(_userDbId);
    if (it != _userToApiKeyMap.end()) {
        // userDbId found in User cache
        std::shared_ptr<Login> login = it->second.first;
        return login->tokenUpdateDurationFromNow();
    } else {
        LOG_WARN(_logger, "User cache not set for userDbId=" << _userDbId);
        return 0;
    }
}
} // namespace KDC
