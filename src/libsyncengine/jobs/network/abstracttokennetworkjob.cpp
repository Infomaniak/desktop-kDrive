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

#define API_PREFIX_DRIVE "/drive"
#define API_PREFIX_PROFILE "/profile"

#define ABSTRACTTOKENNETWORKJOB_NEW_ERROR_MSG "Failed to create AbstractTokenNetworkJob instance!"
#define ABSTRACTTOKENNETWORKJOB_NEW_ERROR_MSG_INVALID_TOKEN "Invalid Token"
#define ABSTRACTTOKENNETWORKJOB_EXEC_ERROR_MSG "Failed to execute AbstractTokenNetworkJob!"

#define TOKEN_LIFETIME 7200  // 2 hours

namespace KDC {

std::unordered_map<int, std::pair<std::shared_ptr<Login>, int>> AbstractTokenNetworkJob::_userToApiKeyMap;
std::unordered_map<int, std::pair<int, int>> AbstractTokenNetworkJob::_driveToApiKeyMap;

AbstractTokenNetworkJob::AbstractTokenNetworkJob(ApiType apiType, int userDbId, int userId, int driveDbId, int driveId,
                                                 bool returnJson)
    : _jsonRes(nullptr),
      _octetStreamRes(std::string()),
      _apiType(apiType),
      _userDbId(userDbId),
      _userId(userId),
      _driveDbId(driveDbId),
      _driveId(driveId),
      _returnJson(returnJson),
      _token(std::string()),
      _errorCode(std::string()),
      _errorDescr(std::string()),
      _error(nullptr) {
    if (!ParmsDb::instance()) {
        LOG_WARN(_logger, "ParmsDb must be initialized!");
        throw std::runtime_error(ABSTRACTTOKENNETWORKJOB_NEW_ERROR_MSG);
    }

    if (((_apiType == ApiDrive || _apiType == ApiNotifyDrive) && _driveDbId == 0 && (_userDbId == 0 || _driveId == 0)) ||
        ((_apiType == ApiProfile || _apiType == ApiDriveByUser) && _userDbId == 0)) {
        LOG_WARN(_logger, "Invalid parameters!");
        throw std::runtime_error(ABSTRACTTOKENNETWORKJOB_NEW_ERROR_MSG);
    }

    _token = loadToken();

    addRawHeader("Authorization", "Bearer " + _token);
}

AbstractTokenNetworkJob::AbstractTokenNetworkJob(ApiType apiType, bool returnJson)
    : AbstractTokenNetworkJob(apiType, 0, 0, 0, 0, returnJson) {}

bool AbstractTokenNetworkJob::hasErrorApi(std::string *errorCode, std::string *errorDescr) {
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

ExitCause AbstractTokenNetworkJob::getExitCause() {
    if (_exitCause == ExitCauseUnknown) {
        if (!_errorCode.empty()) {
            return ExitCauseApiErr;
        } else {
            if (getStatusCode() == HTTPResponse::HTTP_FORBIDDEN) {
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
    }

    return str;
}

bool AbstractTokenNetworkJob::handleError(std::istream &is, const Poco::URI &uri) {
    if (_resHttp.getStatus() == HTTPResponse::HTTP_UNAUTHORIZED) {
        // There is no longer any refresh of the token since v3.5.6
        // This code is only used when updating from a version < v3.5.6
        std::string token = loadToken();
        if (token != _token) {
            LOG_DEBUG(_logger, "Token refreshed by another request");
            _accessTokenAlreadyRefreshed = false;
            _token = token;
            addRawHeader("Authorization", "Bearer " + _token);
            _exitCode = ExitCodeTokenRefreshed;
            return true;
        } else if (!_accessTokenAlreadyRefreshed || tokenUpdateDurationFromNow() > TOKEN_LIFETIME) {
            // The token has not already been refreshed or was refreshed more than its lifetime ago
            if (!refreshToken()) {
                LOG_WARN(_logger, "Refresh token failed");
                noRetry();
                _exitCode = ExitCodeInvalidToken;
                return false;
            } else {
                LOG_DEBUG(_logger, "Refresh token succeeded");
                _exitCode = ExitCodeTokenRefreshed;
                return true;
            }
        } else {
            LOG_WARN(_logger, "Token already refreshed once");
            _exitCode = ExitCodeInvalidToken;
            return false;
        }
    }

    if (_resHttp.getStatus() == HTTPResponse::HTTP_NOT_FOUND) {
        _exitCode = ExitCodeBackError;
        _exitCause = ExitCauseNotFound;
        return false;
    } else {
        // Manage Content-Encoding
        std::stringstream ss;
        std::string encoding = std::string();
        try {
            encoding = _resHttp.get("Content-Encoding");
        } catch (...) {
            // No Content-Encoding
        }

        if (encoding == "gzip") {
            unzip(is, ss);
        } else {
            ss << is.rdbuf();
        }

        Poco::JSON::Parser jsonParser;
        try {
            _error = jsonParser.parse(ss.str()).extract<Poco::JSON::Object::Ptr>();
        } catch (Poco::Exception &exc) {
            LOGW_WARN(_logger, L"Reply " << jobId() << L" received doesn't contain a valid JSON error: "
                                         << Utility::s2ws(exc.displayText()).c_str());
            Utility::logGenericServerError("Request error", ss, _resHttp);

            _exitCode = ExitCodeBackError;
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
            _exitCode = ExitCodeBackError;
            return false;
        }
        if (!JsonParserUtility::extractValue(errorObj, descriptionKey, _errorDescr)) {
            _exitCode = ExitCodeBackError;
            return false;
        }

        if (_errorCode == notAuthorized && !_accessTokenAlreadyRefreshed) {
            LOG_DEBUG(_logger, "Request failed: " << uri.toString().c_str() << " : " << _errorCode.c_str() << " - "
                                                  << _errorDescr.c_str() << ". Refreshing access token.");

            if (!refreshToken()) {
                LOG_WARN(_logger, "Refresh token failed");
                _exitCode = ExitCodeInvalidToken;
                return false;
            } else {
                LOG_DEBUG(_logger, "Refresh token succeeded");
                _exitCode = ExitCodeTokenRefreshed;
                return true;
            }
        } else if (_errorCode == productMaintenance || _errorCode == driveIsInMaintenanceError) {
            LOG_DEBUG(_logger, "Product in maintenance");
            noRetry();
            Poco::JSON::Object::Ptr contextObj = errorObj->getObject(contextKey);
            if (contextObj) {
                std::string context;
                JsonParserUtility::extractValue(contextObj, reasonKey, context, false);

                if (context == notRenew) {
                    _exitCode = ExitCodeBackError;
                    _exitCause = ExitCauseDriveNotRenew;
                    return false;
                }
            }

            _exitCode = ExitCodeBackError;
            _exitCause = ExitCauseDriveMaintenance;
            return false;
        } else if (_errorCode == validationFailed) {
            LOG_DEBUG(_logger, "Invalid file/directory name");
            _exitCode = ExitCodeBackError;
            _exitCause = ExitCauseInvalidName;
            return true;
        } else if (_errorCode == uploadNotTerminatedError) {
            LOG_DEBUG(_logger, "Upload not terminated");
            _exitCode = ExitCodeBackError;
            _exitCause = ExitCauseUploadNotTerminated;
            return true;
        } else if (_errorCode == uploadError && _errorDescr == storageObjectIsNotOk) {
            LOG_DEBUG(_logger, "Upload failed");
            _exitCode = ExitCodeBackError;
            _exitCause = ExitCauseApiErr;
            return true;
        } else if (_errorCode == destinationAlreadyExists || _errorCode == conflictError) {
            LOG_DEBUG(_logger, "Operation refused");
            _exitCode = ExitCodeBackError;
            _exitCause = ExitCauseFileAlreadyExist;
            return true;
        } else if (_errorCode == accessDenied) {
            LOG_DEBUG(_logger, "Access denied");
            _exitCode = ExitCodeBackError;
            _exitCause = ExitCauseHttpErrForbidden;
            return true;
        } else if (_errorCode == fileTooBigError) {
            LOG_DEBUG(_logger, "File too big");
            _exitCode = ExitCodeBackError;
            _exitCause = ExitCauseFileTooBig;
            return true;
        } else {
            LOG_WARN(_logger, "Error in request " << uri.toString().c_str() << " : " << _errorCode.c_str() << " - "
                                                  << _errorDescr.c_str());
            _exitCode = ExitCodeBackError;
            _exitCause = ExitCauseHttpErr;
            return false;
        }
    }

    // Unreachable code
    _exitCode = ExitCodeOk;
    return true;
}

std::string AbstractTokenNetworkJob::getUrl() {
    std::string apiUrl;
    switch (_apiType) {
        case ApiDrive:
        case ApiDriveByUser:
            apiUrl = KDRIVE_API_V2_URL;
            break;
        case ApiNotifyDrive:
            apiUrl = NOTIFY_KDRIVE_V2_URL;
            break;
        case ApiProfile:
            apiUrl = GLOBAL_API_V2_URL;
            break;
    }
    return apiUrl + getSpecificUrl();
}

bool AbstractTokenNetworkJob::handleResponse(std::istream &is) {
    if (_returnJson) {
        return handleJsonResponse(is);
    } else {
        return handleOctetStreamResponse(is);
    }
}

bool AbstractTokenNetworkJob::handleJsonResponse(std::istream &is) {
    // Extract JSON
    Poco::JSON::Parser jsonParser;
    try {
        _jsonRes = jsonParser.parse(is).extract<Poco::JSON::Object::Ptr>();
    } catch (Poco::Exception &exc) {
        LOG_DEBUG(_logger,
                  "Reply " << jobId() << " received doesn't contain a valid JSON payload: " << exc.displayText().c_str());
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
        Poco::JSON::Object::Ptr dataObj = _jsonRes->getObject(dataKey);
        if (dataObj) {
            std::string maintenanceReason;
            if (!JsonParserUtility::extractValue(dataObj, maintenanceReasonKey, maintenanceReason, false)) {
                return false;
            }

            if (maintenanceReason == notRenew) {
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
        LOG_DEBUG(_logger, "Reply " << jobId() << " received doesn't contain a valid JSON error: " << exc.displayText().c_str());

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

    switch (_apiType) {
        case ApiDrive:
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

            user.setKeychainKey("");  // Clear the keychainKey
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

}  // namespace KDC
