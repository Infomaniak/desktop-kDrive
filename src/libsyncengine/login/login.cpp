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

#include "login.h"
#include "jobs/network/login/gettokenjob.h"
#include "jobs/network/login/refreshtokenjob.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"
#include "libcommon/keychainmanager/keychainmanager.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

std::unordered_map<int, Login::LoginInfo> Login::_info;

Login::Login() :
    _logger(Log::instance()->getLogger()) {}

Login::Login(const std::string &keychainKey) :
    _logger(Log::instance()->getLogger()),
    _keychainKey(keychainKey) {
    bool found = false;
    if (!KeyChainManager::instance()->readApiToken(_keychainKey, _apiToken, found)) {
        LOG_WARN(_logger, "Failed to read authentification token from keychain");
    }
    if (!found) {
        LOG_DEBUG(_logger, "Authentification token not found for keychainKey=" << _keychainKey);
    }

    _info[_apiToken.userId()] = LoginInfo();
}

Login::~Login() {}

ExitCode Login::requestToken(const std::string &authorizationCode, const std::string &codeVerifier /*= ""*/) {
    LOG_DEBUG(_logger, "Start token request");

    try {
        GetTokenJob job(authorizationCode, codeVerifier);
        const ExitCode exitCode = job.runSynchronously();
        if (exitCode != ExitCode::Ok) {
            LOG_WARN(_logger, "Error in GetTokenJob::runSynchronously: code=" << exitCode);
            _error = std::string();
            _errorDescr = std::string();
            return exitCode;
        }

        LOG_DEBUG(_logger, "job.runSynchronously() done");
        std::string errorCode;
        std::string errorDescr;
        if (job.hasErrorApi(&errorCode, &errorDescr)) {
            LOGW_WARN(_logger, L"Failed to retrieve authentification token. Error : " << KDC::CommonUtility::s2ws(errorCode) << L" - "
                                                                                      << KDC::CommonUtility::s2ws(errorDescr));
            _error = errorCode;
            _errorDescr = errorDescr;
            return ExitCode::BackError;
        }

        LOG_DEBUG(_logger, "job.hasErrorApi done");
        _apiToken = job.apiToken();
    } catch (const std::runtime_error &e) {
        LOG_WARN(_logger, "Error in GetTokenJob::GetTokenJob: error=" << e.what());
        return ExitCode::SystemError;
    }

    LOG_DEBUG(_logger, "KeyChainManager.writeToken");
    if (!KeyChainManager::instance()->writeToken(_keychainKey, _apiToken.rawData())) {
        LOG_WARN(_logger, "Failed to write authentification token into keychain");
        _error = std::string("Failed to write authentification token into keychain");
        _errorDescr = std::string();
        return ExitCode::SystemError;
    }
    LOG_DEBUG(_logger, "KeyChainManager.writeToken done");

    return ExitCode::Ok;
}

ExitCode Login::refreshToken() {
    return refreshToken(_keychainKey, _apiToken, _error, _errorDescr);
}

long Login::tokenUpdateDurationFromNow() const {
    auto it = _info.find(_apiToken.userId());
    if (it != _info.end()) {
        auto tokenLastUpdate = it->second._lastTokenUpdateTime;
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - tokenLastUpdate);
        return duration.count();
    } else {
        return 0;
    }
}

ExitCode Login::refreshToken(const std::string &keychainKey, ApiToken &apiToken, std::string &error, std::string &errorDescr) {
    LOG_DEBUG(Log::instance()->getLogger(), "Try to refresh token...");

    if (apiToken.refreshToken().empty()) {
        LOG_INFO(Log::instance()->getLogger(), "No refresh token available, user will be asked to log in once again.");
        return ExitCode::InvalidToken;
    }

    std::chrono::time_point<std::chrono::steady_clock> tokenLastUpdate = _info[apiToken.userId()]._lastTokenUpdateTime;

    const std::lock_guard<std::mutex> lock(_info[apiToken.userId()]._mutex);

    if (_info[apiToken.userId()]._lastTokenUpdateTime > tokenLastUpdate) {
        LOG_INFO(Log::instance()->getLogger(), "Token already refreshed in another thread");
        return ExitCode::Ok;
    }

    LOG_DEBUG(Log::instance()->getLogger(), "Start token refresh request");

    if (!KeyChainManager::instance()->writeDummyTest()) {
        error = "Test writing into the keychain failed. Token not refreshed.";
        return ExitCode::SystemError;
    }

    LOG_DEBUG(Log::instance()->getLogger(), "Dummy test passed");

    try {
        RefreshTokenJob job(apiToken);
        const ExitCode exitCode = job.runSynchronously();
        if (exitCode != ExitCode::Ok) {
            LOG_WARN(Log::instance()->getLogger(), "Error in RefreshTokenJob::runSynchronously: code=" << exitCode);
            job.hasErrorApi(&error, &errorDescr);
            return exitCode;
        }

        if (job.hasErrorApi(&error, &errorDescr)) {
            LOG_WARN(Log::instance()->getLogger(),
                     "Failed to retrieve authentication token. Error : " << error << " - " << errorDescr);
            return ExitCode::NetworkError;
        }

        apiToken = job.apiToken();
    } catch (const std::runtime_error &e) {
        LOG_WARN(Log::instance()->getLogger(), "Error in RefreshTokenJob::RefreshTokenJob: error=" << e.what());
        return ExitCode::SystemError;
    }

    LOG_DEBUG(Log::instance()->getLogger(), "Token successfully refreshed");

    if (!KeyChainManager::instance()->writeToken(keychainKey, apiToken.rawData())) {
        LOG_WARN(Log::instance()->getLogger(), "Failed to write authentication token into keychain");
        error = std::string("Failed to write authentication token into keychain");
        errorDescr = std::string();
        return ExitCode::SystemError;
    }

    LOG_DEBUG(Log::instance()->getLogger(), "Token successfully written in keychain");

    KeyChainManager::instance()->clearDummyTest();

    LOG_DEBUG(Log::instance()->getLogger(), "Dummy test cleared");

    _info[apiToken.userId()]._lastTokenUpdateTime = std::chrono::steady_clock::now();

    return ExitCode::Ok;
}

Login::LoginInfo &Login::LoginInfo::operator=(const LoginInfo &info) {
    if (this != &info) {
        _lastTokenUpdateTime = info._lastTokenUpdateTime;
    }

    return *this;
}

} // namespace KDC
