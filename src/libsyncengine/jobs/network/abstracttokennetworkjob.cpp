/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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
#include "jobexceptions.h"

#include "config.h"
#include "utility/urlhelper.h"

#include "libparms/db/parmsdb.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/keychainmanager/keychainmanager.h"
#include "libcommonserver/utility/jsonparserutility.h"

#include <unordered_map>

#include <log4cplus/loggingmacros.h>

#include <Poco/JSON/Parser.h>

constexpr char API_PREFIX_DRIVE[] = "/drive";
constexpr char API_PREFIX_DESKTOP[] = "/desktop";
constexpr int TOKEN_LIFETIME = 7200; // 2 hours

namespace KDC {

std::recursive_mutex AbstractTokenNetworkJob::_cacheMutex;
AbstractTokenNetworkJob::UserCache AbstractTokenNetworkJob::_userToApiKeyMap;
AbstractTokenNetworkJob::DriveCache AbstractTokenNetworkJob::_driveToApiKeyMap;


void AbstractTokenNetworkJob::checkParametersValidity() {
    bool areParametersInvalid = (_apiType == ApiType::Drive || _apiType == ApiType::NotifyDrive) && _driveDbId == 0 &&
                                (_userDbId == 0 || _driveId == 0);
    areParametersInvalid =
            areParametersInvalid || ((_apiType == ApiType::Profile || _apiType == ApiType::DriveByUser) && _userDbId == 0);

    if (areParametersInvalid) {
        assert(false);
        constexpr auto errorMsg = "Invalid parameters in AbstractTokenNetworkJob constructor.";
        LOG_WARN(_logger, errorMsg);
        throw InvalidArgumentError(errorMsg);
    }
}

AbstractTokenNetworkJob::AbstractTokenNetworkJob(const ApiType apiType, const UserDbId userDbId, const UserId userId,
                                                 const DriveDbId driveDbId, const DriveId driveId,
                                                 const bool returnJson /*= true*/) :
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

    checkParametersValidity();
    _apiToken = loadApiToken();

    addRawHeader("Authorization", "Bearer " + _apiToken.accessToken());
}

AbstractTokenNetworkJob::AbstractTokenNetworkJob(const ApiType apiType, const bool returnJson /*= true*/) :
    AbstractTokenNetworkJob(apiType, 0, 0, 0, 0, returnJson) {}

ExitCause AbstractTokenNetworkJob::getExitCause() const {
    if (exitInfo().cause() == ExitCause::Unknown) {
        if (!_backError.code().empty()) {
            return ExitCause::ApiErr;
        }
        if (getStatusCode() == Poco::Net::HTTPResponse::HTTP_FORBIDDEN) {
            return ExitCause::HttpErrForbidden;
        }
        return ExitCause::HttpErr;
    }
    return exitInfo().cause();
}

void AbstractTokenNetworkJob::updateLoginByUserDbId(const Login &login, const UserDbId userDbId) {
    const std::scoped_lock lock(_cacheMutex);
    if (const auto it = _userToApiKeyMap.find(userDbId); it != _userToApiKeyMap.end()) {
        const std::shared_ptr<Login> currentLogin = it->second.login;
        // get new credentials
        const ApiToken &newApiToken = login.apiToken();
        const std::string &newKeychainKey = login.keychainKey();
        // set new credentials to Login class
        currentLogin->setApiToken(newApiToken);
        currentLogin->setKeychainKey(newKeychainKey);
    }
}

void AbstractTokenNetworkJob::clearCache() {
    const std::scoped_lock lock(_cacheMutex);
    _driveToApiKeyMap.clear();
    _userToApiKeyMap.clear();
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
        case ApiType::Desktop:
            str += API_PREFIX_DESKTOP;
            break;
        case ApiType::Profile:
            break;
    }

    return str;
}

ExitInfo AbstractTokenNetworkJob::handleUnauthorizedResponse() {
    if (_apiType == ApiType::Profile || _apiType == ApiType::DriveByUser) {
        return handleUserUnauthorizedResponse();
    } else {
        return handleDriveUnauthorizedResponse();
    }
}

ExitInfo AbstractTokenNetworkJob::handleUserUnauthorizedResponse() {
    // There is no longer any refresh of the token since v3.5.6
    // This code is only used when updating from a version < v3.5.6
    if (const auto apiToken = loadApiToken(); apiToken != _apiToken) {
        LOG_DEBUG(_logger, "Token refreshed by another request");
        _accessTokenAlreadyRefreshed = false;
        _apiToken = apiToken;
        addRawHeader("Authorization", "Bearer " + _apiToken.accessToken());
        return ExitCode::TokenRefreshed;
    }

    if (!_apiToken.refreshToken().empty() && (!_accessTokenAlreadyRefreshed || tokenUpdateDurationFromNow() > TOKEN_LIFETIME)) {
        // The token has not already been refreshed or was refreshed more than its lifetime ago
        if (const auto exitInfo = refreshToken(); !exitInfo) {
            LOG_WARN(_logger, "Refresh token failed");
            disableRetry();
            return ExitCode::InvalidToken;
        }

        LOG_DEBUG(_logger, "Refresh token succeeded");
        return ExitCode::TokenRefreshed;
    }

    if (_trials > 2) disableRetry();

    return ExitCode::InvalidToken;
}

ExitInfo AbstractTokenNetworkJob::handleDriveUnauthorizedResponse() {
    return {ExitCode::BackError, ExitCause::DriveAccessError};
}

void AbstractTokenNetworkJob::defaultBackErrorHandling(const NetworkErrorCode errorCode, const Poco::URI &uri,
                                                       ExitCause &exitCause) {
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
        LOG_WARN(_logger, "Error in request " << Utility::formatRequest(uri, _backError.code(), _backError.description()));
        exitCause = ExitCause::HttpErr;
        return;
    }
    // Regular handling
    const auto &exitHandler = errorHandling->second;
    LOG_DEBUG(_logger, exitHandler.debugMessage);
    exitCause = exitHandler.exitCause;
}


ExitInfo AbstractTokenNetworkJob::handleError(const std::string &replyBody, const Poco::URI &uri) {
    Poco::JSON::Object::Ptr jsonObj;
    if (!replyBody.empty()) {
        if (const auto exitInfo = extractJson(replyBody, jsonObj); !exitInfo) return exitInfo;
    }
    _backError = BackError(jsonObj);

    switch (httpResponse().getStatus()) {
        case Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED:
            return handleUnauthorizedResponse();
        case Poco::Net::HTTPResponse::HTTP_NOT_FOUND: {
            disableRetry();
            const auto exitCause = _backError.contextModel() == "Drive" ? ExitCause::DriveAccessError : ExitCause::NotFound;
            return {ExitCode::BackError, exitCause};
        }
        default:
            break;
    }

    ExitInfo exitInfo = ExitCode::BackError;

    switch (const NetworkErrorCode errorCode = getNetworkErrorCode(_backError.code()); errorCode) {
        case NetworkErrorCode::NotAuthorized: {
            if (!_accessTokenAlreadyRefreshed) {
                LOG_DEBUG(_logger, "Request failed: " << Utility::formatRequest(uri, _backError.code(), _backError.description())
                                                      << ". Refreshing access token.");

                if (!refreshToken()) {
                    LOG_WARN(_logger, "Refresh token failed");
                    exitInfo = ExitCode::InvalidToken;
                    return exitInfo;
                }

                LOG_DEBUG(_logger, "Refresh token succeeded");
                return ExitCode::TokenRefreshed;
            }
            break;
        }
        case NetworkErrorCode::ProductMaintenance:
        case NetworkErrorCode::DriveIsInMaintenanceError: {
            LOG_DEBUG(_logger, "Product in maintenance");
            disableRetry();

            if (getNetworkErrorReason(_backError.contextReason()) == NetworkErrorReason::NotRenew) {
                exitInfo.setCause(ExitCause::DriveNotRenew);
                return exitInfo;
            }
            exitInfo.setCause(ExitCause::DriveMaintenance);
            break;
        }
        default:
            ExitCause exitCause = ExitCause::Unknown;
            defaultBackErrorHandling(errorCode, uri, exitCause);
            exitInfo.setCause(exitCause);
            break;
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
        return handleJsonResponse(replyBody);
    }
    return handleOctetStreamResponse(is);
}

ExitInfo AbstractTokenNetworkJob::handleJsonResponse(const std::string &replyBody) {
    if (const auto exitInfo = AbstractNetworkJob::handleJsonResponse(replyBody); !exitInfo) return exitInfo;

    // Check for maintenance error
    if (!jsonRes()) return {};

    if (const Poco::JSON::Object::Ptr dataObj = jsonRes()->getObject(dataKey); dataObj != nullptr) {
        std::string maintenanceReason;
        if (!JsonParserUtility::extractValue(dataObj, maintenanceReasonKey, maintenanceReason, false)) {
            return {};
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

void AbstractTokenNetworkJob::loadUserInfoFromUserDbId() {
    assert(_userDbId && "Invalid user DB ID.");

    const std::scoped_lock lock(_cacheMutex);

    if (_userToApiKeyMap.contains(_userDbId)) return;

    // Get user
    User user;
    bool found = false;
    if (!ParmsDb::instance()->selectUser(_userDbId, user, found)) {
        assert(false);
        const std::string err{"Error in ParmsDb::selectUser"};
        LOG_WARN(_logger, err);
        throw DbError(err);
    }
    if (!found) {
        assert(false);
        const std::string err{"User not found for userDbId=" + std::to_string(_userDbId)};
        LOG_WARN(_logger, err);
        throw DataError(err);
    }

    if (user.keychainKey().empty()) {
        const std::string err{"Access token is empty"};
        LOG_DEBUG(_logger, err);
        throw TokenError(err);
    }

    // Read token form keystore
    auto login = std::make_shared<Login>(user.keychainKey());
    if (!login->hasToken()) {
        const std::string err{"Failed to retrieve access token"};
        LOG_WARN(_logger, err);
        throw TokenError(err);
    }

    _userToApiKeyMap[_userDbId] = {login, user.userId()};
}

Drive AbstractTokenNetworkJob::getDrive(const DriveDbId driveDbId) const {
    assert(driveDbId > 0 && "Invalid drive DB ID.");

    Drive drive;
    bool found = false;
    if (!ParmsDb::instance()->selectDrive(driveDbId, drive, found)) {
        assert(false);
        constexpr auto err{"Error in ParmsDb::selectDrive"};
        LOG_WARN(_logger, err);
        throw DbError(err);
    }

    if (!found) {
        assert(false);
        const std::string err{"Drive not found for driveDbId=" + std::to_string(driveDbId)};
        LOG_WARN(_logger, err);
        throw DataError(err);
    }

    return drive;
}

Account AbstractTokenNetworkJob::getAccount(const Drive &drive) const {
    assert(drive.dbId() > 0 && "Invalid drive DB ID.");

    Account account;
    bool found = false;
    if (!ParmsDb::instance()->selectAccount(drive.accountDbId(), account, found)) {
        assert(false);
        const std::string err{"Error in ParmsDb::selectAccount"};
        LOG_WARN(_logger, err);
        throw DbError(err);
    }

    if (!found) {
        assert(false);
        const std::string err{"Account not found for accountDbId=" + std::to_string(drive.accountDbId())};
        LOG_WARN(_logger, err);
        throw DataError(err);
    }

    return account;
}

void AbstractTokenNetworkJob::loadUserInfoFromDriveDbId() {
    assert(_driveDbId > 0 && "Invalid drive DB ID.");

    {
        const std::scoped_lock lock(_cacheMutex);

        if (const auto it = _driveToApiKeyMap.find(_driveDbId); it != _driveToApiKeyMap.end()) {
            _userDbId = it->second.userDbId;
            _driveId = it->second.driveId;

            return;
        }
    }

    // Get drive
    const Drive &drive = getDrive(_driveDbId);
    _driveId = drive.driveId();

    // Get account
    const Account &account = getAccount(drive);
    _userDbId = account.userDbId();

    loadUserInfoFromUserDbId();

    const std::scoped_lock lock(_cacheMutex);
    _driveToApiKeyMap[_driveDbId] = {_userDbId, _driveId};
}

ApiToken AbstractTokenNetworkJob::retrieveApiTokenFromUserCache() {
    assert(_userDbId > 0 && "Invalid user DB ID.");

    const std::scoped_lock lock(_cacheMutex);

    if (const auto it = _userToApiKeyMap.find(_userDbId); it == _userToApiKeyMap.cend()) {
        const std::string err{"User cache not set for userDbId=" + std::to_string(_userDbId)};
        LOG_WARN(_logger, err);
        throw std::runtime_error(err);
    } else {
        _userId = it->second.userId;
        return it->second.login->apiToken();
    }
}

ApiToken AbstractTokenNetworkJob::loadApiToken() {
    ApiToken apiToken;
    if (_apiType == ApiType::Desktop) {
        // Fetch the drive identifier of the first available sync.
        std::vector<Sync> syncList;
        if (!ParmsDb::instance()->selectAllSyncs(syncList)) {
            assert(false);
            const std::string err{"Error in ParmsDb::selectAllSyncs"};
            LOG_WARN(_logger, err);
            throw DbError(err);
        }

        if (syncList.empty()) {
            assert(false);
            const std::string err{"No sync found"};
            LOG_WARN(_logger, err);
            throw DataError(err);
        }

        _driveDbId = syncList[0].driveDbId();
    }

    switch (_apiType) {
        case ApiType::Drive:
        case ApiType::Desktop:
        case ApiType::NotifyDrive: {
            if (_driveDbId) loadUserInfoFromDriveDbId();
            apiToken = retrieveApiTokenFromUserCache();
            break;
        }
        case ApiType::Profile:
        case ApiType::DriveByUser: {
            loadUserInfoFromUserDbId();
            apiToken = retrieveApiTokenFromUserCache();
            break;
        }
        default:
            // No token required
            break;
    }

    return apiToken;
}

std::string AbstractTokenNetworkJob::contentType() {
    return mimeTypeJson;
}

ExitInfo AbstractTokenNetworkJob::refreshToken() {
    const std::scoped_lock lock(_cacheMutex);

    _accessTokenAlreadyRefreshed = true;
    const auto it = _userToApiKeyMap.find(_userDbId);
    if (it == _userToApiKeyMap.end()) {
        LOG_WARN(_logger, "User cache not set for userDbId=" << _userDbId);
        return {ExitCode::DataError, ExitCause::LoginError};
    }

    // userDbId found in User cache
    const std::shared_ptr<Login> login = it->second.login;
    if (auto exitInfo = login->refreshToken(); !exitInfo) {
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

        (void) KeyChainManager::instance()->deleteToken(user.keychainKey());

        user.setKeychainKey(""); // Clear the keychainKey
        (void) ParmsDb::instance()->updateUser(user, found);
        if (!found) {
            LOG_WARN(_logger, "User not found for userDbId=" << _userDbId);
            return {ExitCode::DataError, ExitCause::DbEntryNotFound};
        }

        return exitInfo;
    }

    _apiToken = login->apiToken().accessToken();
    addRawHeader("Authorization", "Bearer " + _apiToken.accessToken());
    return ExitCode::Ok;
}

long AbstractTokenNetworkJob::tokenUpdateDurationFromNow() {
    const std::scoped_lock lock(_cacheMutex);
    const auto it = _userToApiKeyMap.find(_userDbId);
    if (it == _userToApiKeyMap.end()) {
        LOG_WARN(_logger, "User cache not set for userDbId=" << _userDbId);
        return 0;
    }
    // userDbId found in User cache
    const std::shared_ptr<Login> login = it->second.login;
    return login->tokenUpdateDurationFromNow();
}

} // namespace KDC
