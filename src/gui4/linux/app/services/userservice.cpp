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

#include "userservice.h"

#include "app/services/sentryservice.h"

#include <QLoggingCategory>

namespace {
constexpr char serviceKeyUser[] = "user";
constexpr char actionLoadAvailableDrives[] = "loadAvailableDrives";
constexpr char actionDeleteUser[] = "deleteUser";
constexpr char actionRequestLoginToken[] = "requestLoginToken";
} // namespace

namespace KDC {

Q_LOGGING_CATEGORY(lcUserService, "gui.v4.userservice", QtInfoMsg)

UserService::UserService(CommService &commService, AppCache &appCache, ServiceActionTracker &serviceActionTracker,
                         ServiceEventBus &serviceEventBus, QObject *const parent) :
    QObject(parent),
    _commService(commService),
    _appCache(appCache),
    _serviceActionTracker(serviceActionTracker),
    _serviceEventBus(serviceEventBus) {
    (void) connect(&_serviceActionTracker, &ServiceActionTracker::servicePendingChanged, this,
                   [this](const ServiceActionTracker::ServiceKey &serviceKey, const bool pending) {
                       if (serviceKey == serviceKeyUser) {
                           setLoading(pending);
                       }
                   });
    (void) connect(&_serviceActionTracker, &ServiceActionTracker::actionPendingChanged, this,
                   [this](const ServiceActionTracker::ServiceKey &serviceKey, const ServiceActionTracker::ActionKey &actionKey,
                          const ServiceActionTracker::ScopeId scopeId, const bool) {
                       if (serviceKey == serviceKeyUser && actionKey == actionLoadAvailableDrives) {
                           emit availableDrivesLoadingChanged(static_cast<UserDbId>(scopeId));
                       }
                   });
    (void) connect(&_appCache, &AppCache::usersChanged, this, [this] { pruneStaleAvailableDriveGenerations(); });
    setLoading(_serviceActionTracker.isServicePending(serviceKeyUser));
}

void UserService::loadAvailableDrives(const qint64 userDbId) {
    const auto scopedUserDbId = static_cast<UserDbId>(userDbId);
    beginAction(actionLoadAvailableDrives, scopedUserDbId);

    // This IPC request cannot be cancelled once sent. Keep a token for the latest valid response, so older callbacks can
    // return without updating the cache. Track pending tokens separately to close the loading state exactly once.
    const uint64_t generation = _nextAvailableDriveLoadGeneration++;
    _availableDriveLoadGenerations[scopedUserDbId] = generation;
    (void) _pendingAvailableDriveLoadGenerations[scopedUserDbId].insert(generation);

    _commService.requestUserAvailableDrives(
            scopedUserDbId,
            [this, scopedUserDbId, generation](const ExitInfo &exitInfo, const std::vector<DriveAvailable> &list) {
                handleAvailableDrivesLoaded(scopedUserDbId, generation, exitInfo, list);
            });
}

void UserService::invalidateAvailableDrivesRequest(const UserDbId userDbId) {
    _availableDriveLoadGenerations[userDbId] = _nextAvailableDriveLoadGeneration++;
    const auto pendingIt = _pendingAvailableDriveLoadGenerations.find(userDbId);
    if (pendingIt == _pendingAvailableDriveLoadGenerations.end()) {
        return;
    }

    endAllActions(actionLoadAvailableDrives, userDbId);
    (void) _pendingAvailableDriveLoadGenerations.erase(pendingIt);
}

void UserService::deleteUser(const qint64 userDbId) {
    beginAction(actionDeleteUser, userDbId);
    invalidateAvailableDrivesRequest(static_cast<UserDbId>(userDbId));

    // Cache consistency is signal-driven: we wait for userRemoved/userUpdated pushes.
    _commService.requestDeleteUser(userDbId, [this, userDbId](const ExitInfo &exitInfo) {
        endAction(actionDeleteUser, userDbId);
        if (!exitInfo) {
            notifyRequestFailure(exitInfo, RequestNum::USER_DELETE);
        }
    });
}

void UserService::requestLoginToken(const QString &code, const QString &codeVerifier) {
    beginAction(actionRequestLoginToken);

    // This IPC request cannot be cancelled once sent. Only the latest token response may continue the login flow. Pending
    // tokens are tracked separately so invalidated requests still close their loading state exactly once.
    const uint64_t generation = ++_loginTokenGeneration;
    (void) _pendingLoginTokenGenerations.insert(generation);

    _commService.requestLoginToken(
            code, codeVerifier, [this, generation](const ExitInfo &exitInfo, const LoginTokenResult &result) {
                if (_pendingLoginTokenGenerations.erase(generation) == 0) {
                    return;
                }
                endAction(actionRequestLoginToken);
                if (generation != _loginTokenGeneration) {
                    qCInfo(lcUserService) << "Stale login token response ignored | generation:" << generation
                                          << "/ activeGeneration:" << _loginTokenGeneration;
                    return;
                }

                if (!result.error.isEmpty() || !result.errorDescription.isEmpty()) {
                    _serviceEventBus.notifyGenericError(exitInfo, RequestNum::LOGIN_REQUESTTOKEN);
                    SentryService::reportError(
                            QStringLiteral("Login failed"),
                            QStringLiteral("error: %1 | description: %2").arg(result.error, result.errorDescription));
                    emit loginTokenFailed(result.error, result.errorDescription);
                    return;
                }

                if (!exitInfo) {
                    notifyRequestFailure(exitInfo, RequestNum::LOGIN_REQUESTTOKEN);
                    SentryService::reportError("Login failed", toString(exitInfo));
                    emit loginTokenFailed(QString(), QString());
                    return;
                }

                emit loginTokenSucceeded(result.userDbId);
            });
}

void UserService::invalidateLoginTokenRequest() {
    ++_loginTokenGeneration;
    if (!_pendingLoginTokenGenerations.empty()) {
        endAllActions(actionRequestLoginToken);
        _pendingLoginTokenGenerations.clear();
    }
}

bool UserService::isLoadAvailableDrivesPending(const qint64 userDbId) const {
    return isActionPending(actionLoadAvailableDrives, userDbId);
}

bool UserService::isDeleteUserPending(const qint64 userDbId) const {
    return isActionPending(actionDeleteUser, userDbId);
}

bool UserService::isLoginPending() const {
    return isActionPending(actionRequestLoginToken);
}

void UserService::pruneStaleAvailableDriveGenerations() {
    for (auto it = _availableDriveLoadGenerations.begin(); it != _availableDriveLoadGenerations.end();) {
        if (_appCache.user(it->first).has_value()) {
            ++it;
            continue;
        }

        if (const auto pendingIt = _pendingAvailableDriveLoadGenerations.find(it->first);
            pendingIt != _pendingAvailableDriveLoadGenerations.end()) {
            endAllActions(actionLoadAvailableDrives, it->first);
            (void) _pendingAvailableDriveLoadGenerations.erase(pendingIt);
        }
        it = _availableDriveLoadGenerations.erase(it);
    }
}

void UserService::handleAvailableDrivesLoaded(const UserDbId userDbId, const uint64_t generation, const ExitInfo &exitInfo,
                                              const std::vector<DriveAvailable> &list) {
    const auto pendingIt = _pendingAvailableDriveLoadGenerations.find(userDbId);
    if (pendingIt == _pendingAvailableDriveLoadGenerations.end() || pendingIt->second.erase(generation) == 0) {
        return;
    }
    if (pendingIt->second.empty()) {
        (void) _pendingAvailableDriveLoadGenerations.erase(pendingIt);
    }
    endAction(actionLoadAvailableDrives, userDbId);
    if (const auto generationIt = _availableDriveLoadGenerations.find(userDbId);
        generationIt == _availableDriveLoadGenerations.end() || generationIt->second != generation) {
        return;
    }

    if (!_appCache.user(userDbId).has_value()) {
        qCWarning(lcUserService) << "Available drives load ignored because user disappeared | userDbId:" << userDbId;
        (void) _availableDriveLoadGenerations.erase(userDbId);
        emit availableDrivesLoadFailed(userDbId);
        return;
    }

    if (!exitInfo) {
        notifyRequestFailure(exitInfo, RequestNum::USER_AVAILABLEDRIVES);
        emit availableDrivesLoadFailed(userDbId);
        return;
    }

    _appCache.replaceAvailableDrivesForUser(userDbId, list);
    emit availableDrivesLoaded(userDbId);
}

void UserService::beginAction(const ServiceActionTracker::ActionKey &actionKey,
                              const ServiceActionTracker::ScopeId scopeId) const {
    _serviceActionTracker.beginAction(serviceKeyUser, actionKey, scopeId);
}

void UserService::endAction(const ServiceActionTracker::ActionKey &actionKey, const ServiceActionTracker::ScopeId scopeId) const {
    _serviceActionTracker.endAction(serviceKeyUser, actionKey, scopeId);
}

void UserService::endAllActions(const ServiceActionTracker::ActionKey &actionKey,
                                const ServiceActionTracker::ScopeId scopeId) const {
    _serviceActionTracker.endAllActions(serviceKeyUser, actionKey, scopeId);
}

bool UserService::isActionPending(const ServiceActionTracker::ActionKey &actionKey,
                                  const ServiceActionTracker::ScopeId scopeId) const {
    return _serviceActionTracker.isActionPending(serviceKeyUser, actionKey, scopeId);
}

void UserService::setLoading(const bool loading) {
    if (_loading == loading) {
        return;
    }
    _loading = loading;
    emit loadingChanged();
}

void UserService::notifyRequestFailure(const ExitInfo &exitInfo, const RequestNum requestNum) {
    qCWarning(lcUserService) << "User service request failed | code:" << exitInfo.code() << "/ cause:" << exitInfo.cause();
    _serviceEventBus.notifyGenericError(exitInfo, requestNum);
}

} // namespace KDC
