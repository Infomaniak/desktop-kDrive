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

#include "abstracttokennetworkjob.h"
#include "config.h"
#include "libcommonserver/utility/utility.h"
#include "libparms/db/parmsdb.h"
#include "libcommon/keychainmanager/keychainmanager.h"
#include "libcommon/utility/jsonparserutility.h"
#include "utility/urlhelper.h"

#include <unordered_map>

#include <log4cplus/loggingmacros.h>

#include <Poco/JSON/Parser.h>

constexpr char API_PREFIX_DRIVE[] = "/drive";
constexpr char API_PREFIX_DESKTOP[] = "/desktop";
constexpr char API_PREFIX_PROFILE[] = "/profile";
constexpr int TOKEN_LIFETIME = 7200; // 2 hours

namespace KDC {
std::unordered_map<int, std::pair<std::shared_ptr<Login>, int>> AbstractTokenNetworkJob::_userToApiKeyMap;
std::unordered_map<int, std::pair<int, int>> AbstractTokenNetworkJob::_driveToApiKeyMap;

AbstractTokenNetworkJob::AbstractTokenNetworkJob(ApiType apiType, int userDbId, int userId, int driveDbId, int driveId,
                                                 bool returnJson /*= true*/) :
    _apiType(apiType),
    _userDbId(userDbId),
    _userId(userId),
    _driveDbId(driveDbId),
    _driveId(driveId),
    _returnJson(returnJson) {
    if (!ParmsDb::instance()) {
        assert(false);
        LOG_WARN(_logger, "ParmsDb must be initialized!");
        throw DbError("ParmsDb must be initialized!");
    }

    if (((_apiType == ApiType::Drive || _apiType == ApiType::NotifyDrive) && _driveDbId == 0 &&
         (_userDbId == 0 || _driveId == 0)) ||
        ((_apiType == ApiType::Profile || _apiType == ApiType::DriveByUser) && _userDbId == 0)) {
        assert(false);
        LOG_WARN(_logger, "Invalid parameters!");
        throw std::runtime_error("Invalid parameters!");
    }

    _token = loadToken();

    addRawHeader("Authorization", "Bearer " + _token);
}

AbstractTokenNetworkJob::AbstractTokenNetworkJob(ApiType apiType, bool returnJson /*= true*/) :
    AbstractTokenNetworkJob(apiType, 0, 0, 0, 0, returnJson) {}

ExitCause AbstractTokenNetworkJob::getExitCause() const {
    if (exitInfo().cause() == ExitCause::Unknown) {
        if (!_errorCode.empty()) {
            return ExitCause::ApiErr;
        }
        if (getStatusCode() == Poco::Net::HTTPResponse::HTTP_FORBIDDEN) {
            return ExitCause::HttpErrForbidden;
        }
        return ExitCause::HttpErr;
    }
    return exitInfo().cause();
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

ExitInfo AbstractTokenNetworkJob::handleUnauthorizedResponse() {
    // There is no longer any refresh of the token since v3.5.6
    // This code is only used when updating from a version < v3.5.6
    if (std::string token = loadToken(); token != _token) {
        LOG_DEBUG(_logger, "Token refreshed by another request");
        _accessTokenAlreadyRefreshed = false;
        _token = token;
        addRawHeader("Authorization", "Bearer " + _token);
        return ExitCode::TokenRefreshed;
    }

    if (!_accessTokenAlreadyRefreshed || tokenUpdateDurationFromNow() > TOKEN_LIFETIME) {
        // The token has not already been refreshed or was refreshed more than its lifetime ago
        if (const auto exitInfo = refreshToken(); !exitInfo) {
            LOG_WARN(_logger, "Refresh token failed");
            disableRetry();
            return ExitCode::InvalidToken;
        }

        LOG_DEBUG(_logger, "Refresh token succeeded");
        return ExitCode::TokenRefreshed;
    }

    LOG_WARN(_logger, "Token already refreshed once");
    return ExitCode::InvalidToken;
}

bool AbstractTokenNetworkJob::defaultBackErrorHandling(NetworkErrorCode errorCode, const Poco::URI &uri, ExitCause &exitCause) {
    static const std::map<NetworkErrorCode, AbstractTokenNetworkJob::ExitHandler> errorCodeHandlingMap = {
            {NetworkErrorCode::ValidationFailed, ExitHandler{ExitCause::InvalidName, "Invalid file or directory name"}},
            {NetworkErrorCode::UploadNotTerminatedError, ExitHandler{ExitCause::UploadNotTerminated, "Upload not terminated"}},
            {NetworkErrorCode::UploadError, ExitHandler{ExitCause::ApiErr, "Upload failed"}},
            {NetworkErrorCode::DestinationAlreadyExists, ExitHandler{ExitCause::FileExists, "Operation refused"}},
            {NetworkErrorCode::ConflictError, ExitHandler{ExitCause::FileExists, "Operation refused"}},
            {NetworkErrorCode::AccessDenied, ExitHandler{ExitCause::HttpErrForbidden, "Access denied"}},
            {NetworkErrorCode::FileTooBigError, ExitHandler{ExitCause::FileTooBig, "File too big"}},
            {NetworkErrorCode::QuotaExceededError, ExitHandler{ExitCause::QuotaExceeded, "Quota exceeded"}},
            {NetworkErrorCode::FileShareLinkAlreadyExists,
             ExitHandler{ExitCause::ShareLinkAlreadyExists, "Share link already exists"}},
            {NetworkErrorCode::LockError, ExitHandler{ExitCause::FileLocked, "File is locked by an other user"}}

    };

    const auto &errorHandling = errorCodeHandlingMap.find(errorCode);
    if (errorHandling == errorCodeHandlingMap.cend()) {
        LOG_WARN(_logger, "Error in request " << Utility::formatRequest(uri, _errorCode, _errorDescr));
        exitCause = ExitCause::HttpErr;
        return false;
    }
    // Regular handling
    const auto &exitHandler = errorHandling->second;
    LOG_DEBUG(_logger, exitHandler.debugMessage);
    exitCause = exitHandler.exitCause;

    return true;
}


ExitInfo AbstractTokenNetworkJob::handleError(const std::string &replyBody, const Poco::URI &uri) {
    switch (_resHttp.getStatus()) {
        case Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED:
            return handleUnauthorizedResponse();
        case Poco::Net::HTTPResponse::HTTP_NOT_FOUND: {
            disableRetry();
            return {ExitCode::BackError, ExitCause::NotFound};
        }
        default:
            break;
    }

    ExitInfo exitInfo = ExitCode::BackError;

    Poco::JSON::Object::Ptr errorObjPtr = nullptr;
    if (!extractJsonError(replyBody, errorObjPtr)) return exitInfo;

    const NetworkErrorCode errorCode = getNetworkErrorCode(_errorCode);
    switch (errorCode) {
        case NetworkErrorCode::NotAuthorized: {
            if (!_accessTokenAlreadyRefreshed) {
                LOG_DEBUG(_logger, "Request failed: " << Utility::formatRequest(uri, _errorCode, _errorDescr)
                                                      << ". Refreshing access token.");

                if (!refreshToken()) {
                    LOG_WARN(_logger, "Refresh token failed");
                    exitInfo = ExitCode::InvalidToken;
                    return exitInfo;
                }

                LOG_DEBUG(_logger, "Refresh token succeeded");
                return ExitCode::TokenRefreshed;
            }
            return exitInfo;
        }

        case NetworkErrorCode::ProductMaintenance:
        case NetworkErrorCode::DriveIsInMaintenanceError: {
            LOG_DEBUG(_logger, "Product in maintenance");
            disableRetry();
            if (const auto contextObj = errorObjPtr->getObject(contextKey); contextObj != nullptr) {
                std::string context;
                JsonParserUtility::extractValue(contextObj, reasonKey, context, false);
                if (getNetworkErrorReason(context) == NetworkErrorReason::NotRenew) {
                    exitInfo.setCause(ExitCause::DriveNotRenew);
                    return exitInfo;
                }
            }
            exitInfo.setCause(ExitCause::DriveMaintenance);
            return exitInfo;
        }
        default:
            ExitCause exitCause = ExitCause::Unknown;
            bool res = defaultBackErrorHandling(errorCode, uri, exitCause);
            exitInfo.setCause(ExitCause::DriveMaintenance);
            return exitInfo;
    }
    return exitInfo;
}

std::string AbstractTokenNetworkJob::getUrl() {
    std::string apiUrl;
    switch (_apiType) {
        case ApiType::Drive:
        case ApiType::DriveByUser:
        case ApiType::Desktop:
            apiUrl = UrlHelper::kDriveApiUrl(_apiVersion);
            break;
        case ApiType::NotifyDrive:
            apiUrl = UrlHelper::notifyApiUrl(_apiVersion);
            break;
        case ApiType::Profile:
            apiUrl = UrlHelper::infomaniakApiUrl(_apiVersion);
            break;
    }
    return apiUrl + getSpecificUrl();
}

ExitInfo AbstractTokenNetworkJob::handleResponse(std::istream &is) {
    if (_returnJson) {
        std::string replyBody;
        getStringFromStream(is, replyBody);
        if (isExtendedLog()) {
            LOGW_DEBUG(_logger, L"Reply " << jobId() << L" received: " << CommonUtility::s2ws(replyBody));
        }
        return handleJsonResponse(replyBody);
    }
    return handleOctetStreamResponse(is);
}

ExitInfo AbstractTokenNetworkJob::handleJsonResponse(const std::string &replyBody) {
    if (const auto exitInfo = AbstractNetworkJob::handleJsonResponse(replyBody); !exitInfo) return exitInfo;

    // Check for maintenance error
    if (!jsonRes()) return ExitInfo();

    if (const Poco::JSON::Object::Ptr dataObj = jsonRes()->getObject(dataKey); dataObj != nullptr) {
        std::string maintenanceReason;
        if (!JsonParserUtility::extractValue(dataObj, maintenanceReasonKey, maintenanceReason, false)) {
            return ExitInfo();
        }

        if (getNetworkErrorReason(maintenanceReason) == NetworkErrorReason::NotRenew) {
            disableRetry();
            return {ExitCode::BackError, ExitCause::DriveNotRenew};
        }
        if (getNetworkErrorReason(maintenanceReason) == NetworkErrorReason::Technical) {
            ExitInfo exitInfo = ExitCode::BackError;
            const auto maintenanceTypesArray = JsonParserUtility::extractArrayObject(dataObj, maintenanceTypesKey);
            if (!maintenanceTypesArray || maintenanceTypesArray->empty()) return exitInfo;

            for (const auto &maintenanceInfoVar: *maintenanceTypesArray) {
                const auto &maintenanceInfoObj = maintenanceInfoVar.extract<Poco::JSON::Object::Ptr>();
                if (std::string val; JsonParserUtility::extractValue(maintenanceInfoObj, codeKey, val) && val == "asleep") {
                    LOG_DEBUG(_logger, "Drive is asleep");
                    exitInfo.setCause(ExitCause::DriveAsleep);
                    disableRetry();
                    return exitInfo;
                }
                if (std::string val; JsonParserUtility::extractValue(maintenanceInfoObj, codeKey, val) && val == "waking_up") {
                    LOG_DEBUG(_logger, "Drive is waking up");
                    exitInfo.setCause(ExitCause::DriveWakingUp);
                    disableRetry();
                    return exitInfo;
                }
            }

            return exitInfo;
        }
    }

    return ExitCode::Ok;
}

std::string AbstractTokenNetworkJob::loadToken() {
    std::string token;
    if (_apiType == ApiType::Desktop) {
        // Fetch the drive identifier of the first available sync.
        std::vector<Sync> syncList;
        if (!ParmsDb::instance()->selectAllSyncs(syncList)) {
            assert(false);
            std::string err{"Error in ParmsDb::selectAllSyncs"};
            LOG_WARN(_logger, err);
            throw DbError(err);
        }

        if (syncList.empty()) {
            assert(false);
            std::string err{"No sync found"};
            LOG_WARN(_logger, err);
            throw DataError(err);
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
                        assert(false);
                        std::string err{"Error in ParmsDb::selectDrive"};
                        LOG_WARN(_logger, err);
                        throw DbError(err);
                    }
                    if (!found) {
                        assert(false);
                        std::string err{"Drive not found for driveDbId=" + std::to_string(_driveDbId)};
                        LOG_WARN(_logger, err);
                        throw DataError(err);
                    }

                    _driveId = drive.driveId();

                    // Get account
                    Account account;
                    if (!ParmsDb::instance()->selectAccount(drive.accountDbId(), account, found)) {
                        assert(false);
                        std::string err{"Error in ParmsDb::selectAccount"};
                        LOG_WARN(_logger, err);
                        throw DbError(err);
                    }
                    if (!found) {
                        assert(false);
                        std::string err{"Account not found for accountDbId=" + std::to_string(drive.accountDbId())};
                        LOG_WARN(_logger, err);
                        throw DataError(err);
                    }

                    _userDbId = account.userDbId();

                    if (_userToApiKeyMap.find(_userDbId) == _userToApiKeyMap.end()) {
                        // Get user
                        User user;
                        if (!ParmsDb::instance()->selectUser(_userDbId, user, found)) {
                            assert(false);
                            std::string err{"Error in ParmsDb::selectUser"};
                            LOG_WARN(_logger, err);
                            throw DbError(err);
                        }
                        if (!found) {
                            assert(false);
                            std::string err{"User not found for userDbId=" + std::to_string(_userDbId)};
                            LOG_WARN(_logger, err);
                            throw DataError(err);
                        }

                        if (user.keychainKey().empty()) {
                            std::string err{"Access token is empty"};
                            LOG_DEBUG(_logger, err);
                            throw TokenError(err);
                        }

                        // Read token form keystore
                        std::shared_ptr<Login> login = std::shared_ptr<Login>(new Login(user.keychainKey()));
                        if (!login->hasToken()) {
                            std::string err{"Failed to retrieve access token"};
                            LOG_WARN(_logger, err);
                            throw TokenError(err);
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
                assert(false);
                std::string err{"User cache not set for userDbId=" + std::to_string(_userDbId)};
                LOG_WARN(_logger, err);
                throw std::runtime_error(err);
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
                    assert(false);
                    std::string err{"Error in ParmsDb::selectUser"};
                    LOG_WARN(_logger, err);
                    throw DbError(err);
                }
                if (!found) {
                    assert(false);
                    std::string err{"User not found for userDbId=" + std::to_string(_userDbId)};
                    LOG_WARN(_logger, err);
                    throw DataError(err);
                }

                if (user.keychainKey().empty()) {
                    std::string err{"Access token is empty"};
                    LOG_DEBUG(_logger, err);
                    throw TokenError(err);
                }

                // Read token form keystore
                std::shared_ptr<Login> login = std::shared_ptr<Login>(new Login(user.keychainKey()));
                if (!login->hasToken()) {
                    std::string err{"Failed to retrieve access token"};
                    LOG_WARN(_logger, err);
                    throw TokenError(err);
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

std::string AbstractTokenNetworkJob::getContentType() {
    return mimeTypeJson;
}

ExitInfo AbstractTokenNetworkJob::refreshToken() {
    _accessTokenAlreadyRefreshed = true;

    auto it = _userToApiKeyMap.find(_userDbId);
    if (it != _userToApiKeyMap.end()) {
        // userDbId found in User cache
        std::shared_ptr<Login> login = it->second.first;
        ExitInfo exitInfo = login->refreshToken();
        if (!exitInfo) {
            LOG_WARN(_logger, "Failed to refresh token: code=" << exitInfo << " login error=" << login->error()
                                                               << " login error descr=" << login->errorDescr());
            exitInfo.setCause(ExitCause::LoginError);

            // Clear the keychain key
            User user;
            bool found = false;
            if (!ParmsDb::instance()->selectUser(_userDbId, user, found)) {
                LOG_WARN(_logger, "Error in ParmsDb::selectUser");
                return ExitCode::DbError;
            }
            if (!found) {
                LOG_WARN(_logger, "User not found for userDbId=" << _userDbId);
                return {ExitCode::DataError, ExitCause::DbEntryNotFound};
            }

            KeyChainManager::instance()->deleteToken(user.keychainKey());

            user.setKeychainKey(""); // Clear the keychainKey
            ParmsDb::instance()->updateUser(user, found);
            if (!found) {
                LOG_WARN(_logger, "User not found for userDbId=" << _userDbId);
                return {ExitCode::DataError, ExitCause::DbEntryNotFound};
            }

            return exitInfo;
        }

        _token = login->apiToken().accessToken();
        addRawHeader("Authorization", "Bearer " + _token);
        return ExitCode::Ok;
    } else {
        LOG_WARN(_logger, "User cache not set for userDbId=" << _userDbId);
        return {ExitCode::DataError, ExitCause::LoginError};
    }

    return ExitCode::Ok;
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

ExitCode AbstractTokenNetworkJob::exception2ExitCode(const std::exception &exc) {
    if (dynamic_cast<const AbstractTokenNetworkJob::DbError *>(&exc)) {
        return ExitCode::DbError;
    } else if (dynamic_cast<const AbstractTokenNetworkJob::DataError *>(&exc)) {
        return ExitCode::DataError;
    } else if (dynamic_cast<const AbstractTokenNetworkJob::TokenError *>(&exc)) {
        return ExitCode::InvalidToken;
    }

    return ExitCode::Unknown;
}

} // namespace KDC
