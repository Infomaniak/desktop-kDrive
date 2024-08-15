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
#include "../networkjobsparams.h"
#include "../../../../libcommonserver/utility/utility.h"
#include "../../../../libparms/db/parmsdb.h"
#include "../../../../libcommon/keychainmanager/keychainmanager.h"
#include "../../../../libcommon/utility/jsonparserutility.h"

#include <unordered_map>

#include <log4cplus/loggingmacros.h>

#include <Poco/JSON/Parser.h>

constexpr char API_PREFIX_DRIVE[] = "/drive";
constexpr char API_PREFIX_DESKTOP[] = "/desktop";
constexpr char API_PREFIX_PROFILE[] = "/profile";

constexpr char ABSTRACTTOKENNETWORKJOB_NEW_ERROR_MSG[] = "Failed to create AbstractTokenNetworkJob instance!";
constexpr char ABSTRACTTOKENNETWORKJOB_NEW_ERROR_MSG_INVALID_TOKEN[] = "Invalid Token";
constexpr char ABSTRACTTOKENNETWORKJOB_EXEC_ERROR_MSG[] = "Failed to execute AbstractTokenNetworkJob!";

constexpr int TOKEN_LIFETIME = 7200;  // 2 hours

namespace KDC {
std::unordered_map<int, std::pair<std::shared_ptr<Login>, int>> AbstractTokenNetworkJob::_userToApiKeyMap;
std::unordered_map<int, std::pair<int, int>> AbstractTokenNetworkJob::_driveToApiKeyMap;

AbstractTokenNetworkJob::AbstractTokenNetworkJob(ApiType apiType, int userDbId, int userId, int driveDbId, int driveId,
                                                 bool returnJson /*= true*/)
    : _apiType(apiType), _userDbId(userDbId), _userId(userId), _driveDbId(driveDbId), _driveId(driveId), _returnJson(returnJson) {
    if (!ParmsDb::instance()) {
        LOG_WARN(_logger, "ParmsDb must be initialized!");
        throw std::runtime_error(ABSTRACTTOKENNETWORKJOB_NEW_ERROR_MSG);
    }

    if (((_apiType == ApiType::Drive || _apiType == ApiType::NotifyDrive) && _driveDbId == 0 &&
         (_userDbId == 0 || _driveId == 0)) ||
        ((_apiType == ApiType::Profile || _apiType == ApiType::DriveByUser) && _userDbId == 0)) {
        LOG_WARN(_logger, "Invalid parameters!");
        throw std::runtime_error(ABSTRACTTOKENNETWORKJOB_NEW_ERROR_MSG);
    }

    _token = loadToken();

    addRawHeader("Authorization", "Bearer " + _token);
}

AbstractTokenNetworkJob::AbstractTokenNetworkJob(ApiType apiType, bool returnJson /*= true*/)
    : AbstractTokenNetworkJob(apiType, 0, 0, 0, 0, returnJson) {}

ExitCause AbstractTokenNetworkJob::getExitCause() {
    if (_exitCause == ExitCause::Unknown) {
        if (!_errorCode.empty()) {
            return ExitCause::ApiErr;
        } else {
            if (getStatusCode() == Poco::Net::HTTPResponse::HTTP_FORBIDDEN) {
                return ExitCause::HttpErrForbidden;
            } else {
                return ExitCause::HttpErr;
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
        case ApiType::Drive:
        case ApiType::NotifyDrive:
            str += API_PREFIX_DRIVE;
            if (_driveId) {
                str += "/";
                str += std::to_string(_driveId);
            }
            break;
        case ApiType::DriveByUser:
            str += API_PREFIX_DRIVE;
            break;
        case ApiType::Profile:
            str += API_PREFIX_PROFILE;
            break;
        case ApiType::Desktop:
            str += API_PREFIX_DESKTOP;
            break;
    }

    return str;
}

bool AbstractTokenNetworkJob::handleUnauthorizedResponse() {
    // There is no longer any refresh of the token since v3.5.6
    // This code is only used when updating from a version < v3.5.6
    _exitCode = ExitCode::InvalidToken;
    if (std::string token = loadToken(); token != _token) {
        LOG_DEBUG(_logger, "Token refreshed by another request");
        _accessTokenAlreadyRefreshed = false;
        _token = token;
        addRawHeader("Authorization", "Bearer " + _token);
        _exitCode = ExitCode::TokenRefreshed;

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
        _exitCode = ExitCode::TokenRefreshed;

        return true;
    }

    LOG_WARN(_logger, "Token already refreshed once");

    return false;
}

bool AbstractTokenNetworkJob::defaultBackErrorHandling(NetworkErrorCode errorCode, const Poco::URI &uri) {
    static const std::map<NetworkErrorCode, AbstractTokenNetworkJob::ExitHandler> errorCodeHandlingMap = {
        {NetworkErrorCode::validationFailed, ExitHandler{ExitCause::InvalidName, "Invalid file or directory name"}},
        {NetworkErrorCode::uploadNotTerminatedError, ExitHandler{ExitCause::UploadNotTerminated, "Upload not terminated"}},
        {NetworkErrorCode::uploadError, ExitHandler{ExitCause::ApiErr, "Upload failed"}},
        {NetworkErrorCode::destinationAlreadyExists, ExitHandler{ExitCause::FileAlreadyExist, "Operation refused"}},
        {NetworkErrorCode::conflictError, ExitHandler{ExitCause::FileAlreadyExist, "Operation refused"}},
        {NetworkErrorCode::accessDenied, ExitHandler{ExitCause::HttpErrForbidden, "Access denied"}},
        {NetworkErrorCode::fileTooBigError, ExitHandler{ExitCause::FileTooBig, "File too big"}},
        {NetworkErrorCode::quotaExceededError, ExitHandler{ExitCause::QuotaExceeded, "Quota exceeded"}}};

    const auto &errorHandling = errorCodeHandlingMap.find(errorCode);
    if (errorHandling == errorCodeHandlingMap.cend()) {
        LOG_WARN(_logger, "Error in request " << Utility::formatRequest(uri, _errorCode, _errorDescr).c_str());
        _exitCause = ExitCause::HttpErr;

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
            _exitCode = ExitCode::BackError;
            _exitCause = ExitCause::NotFound;
            return false;
        }
        default:
            break;
    }

    _exitCode = ExitCode::BackError;

    Poco::JSON::Object::Ptr errorObjPtr = nullptr;
    if (!extractJsonError(is, errorObjPtr)) return false;

    const NetworkErrorCode errorCode = getNetworkErrorCode(_errorCode);
    switch (errorCode) {
        case KDC::NetworkErrorCode::notAuthorized: {
            if (!_accessTokenAlreadyRefreshed) {
                LOG_DEBUG(_logger, "Request failed: " << Utility::formatRequest(uri, _errorCode, _errorDescr).c_str()
                                                      << ". Refreshing access token.");

                if (!refreshToken()) {
                    LOG_WARN(_logger, "Refresh token failed");
                    _exitCode = ExitCode::InvalidToken;
                    return false;
                }

                LOG_DEBUG(_logger, "Refresh token succeeded");
                _exitCode = ExitCode::TokenRefreshed;
                return true;
            }
        }

        case KDC::NetworkErrorCode::productMaintenance:
        case KDC::NetworkErrorCode::driveIsInMaintenanceError: {
            LOG_DEBUG(_logger, "Product in maintenance");
            noRetry();
            if (const auto contextObj = errorObjPtr->getObject(contextKey); contextObj != nullptr) {
                std::string context;
                JsonParserUtility::extractValue(contextObj, reasonKey, context, false);
                if (getNetworkErrorReason(context) == NetworkErrorReason::notRenew) {
                    _exitCause = ExitCause::DriveNotRenew;
                    return false;
                }
            }
            _exitCause = ExitCause::DriveMaintenance;

            return false;
        }
        default:
            return defaultBackErrorHandling(errorCode, uri);
    }
}

std::string AbstractTokenNetworkJob::getUrl() {
    std::string apiUrl;
    switch (_apiType) {
        case ApiType::Drive:
        case ApiType::DriveByUser:
        case ApiType::Desktop:
            apiUrl = KDRIVE_API_V2_URL;
            break;
        case ApiType::NotifyDrive:
            apiUrl = NOTIFY_KDRIVE_V2_URL;
            break;
        case ApiType::Profile:
            apiUrl = GLOBAL_API_V2_URL;
            break;
    }
    return apiUrl + getSpecificUrl();
}

bool AbstractTokenNetworkJob::handleResponse(std::istream &is) {
    if (_returnJson) return handleJsonResponse(is);
    return handleOctetStreamResponse(is);
}

bool AbstractTokenNetworkJob::handleJsonResponse(std::istream &is) {
    if (!AbstractNetworkJob::handleJsonResponse(is)) return false;

    // Check for maintenance error
    if (jsonRes()) {
        if (const Poco::JSON::Object::Ptr dataObj = jsonRes()->getObject(dataKey); dataObj != nullptr) {
            std::string maintenanceReason;
            if (!JsonParserUtility::extractValue(dataObj, maintenanceReasonKey, maintenanceReason, false)) {
                return false;
            }

            if (getNetworkErrorReason(maintenanceReason) == NetworkErrorReason::notRenew) {
                noRetry();
                _exitCode = ExitCode::BackError;
                _exitCause = ExitCause::DriveNotRenew;
                return false;
            }
        }
    }

    return true;
}

std::string AbstractTokenNetworkJob::loadToken() {
    std::string token;
    if (_apiType == ApiType::Desktop) {
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
        case ApiType::Drive:
        case ApiType::Desktop:
        case ApiType::NotifyDrive: {
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
                            _exitCode = ExitCode::InvalidToken;
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
        case ApiType::Profile:
        case ApiType::DriveByUser: {
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
                    _exitCode = ExitCode::InvalidToken;
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
        if (exitCode != ExitCode::Ok) {
            LOG_WARN(_logger, "Failed to refresh token: " << enumClassToInt(exitCode) << " - " << login->error().c_str() << " - "
                                                          << login->errorDescr().c_str());
            _exitCause = ExitCause::LoginError;
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
        _exitCause = ExitCause::LoginError;
        _exitCode = ExitCode::DataError;
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
