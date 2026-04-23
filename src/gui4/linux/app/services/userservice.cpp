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

#include <QLoggingCategory>

namespace {
constexpr char serviceKeyUser[] = "user";
constexpr char actionLoadUsers[] = "loadUsers";
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
    (void) connect(&_commService, &CommService::userAdded, &_appCache, &AppCache::upsertUser);
    (void) connect(&_commService, &CommService::userUpdated, &_appCache, &AppCache::upsertUser);
    (void) connect(&_commService, &CommService::userRemoved, &_appCache, &AppCache::removeUser);
    (void) connect(&_serviceActionTracker, &ServiceActionTracker::servicePendingChanged, this,
                   [this](const QString &serviceKey, const bool pending) {
                       if (serviceKey == serviceKeyUser) {
                           setLoading(pending);
                       }
                   });
    setLoading(_serviceActionTracker.isServicePending(serviceKeyUser));
}

void UserService::loadUsers() {
    beginAction(actionLoadUsers);

    _commService.requestUserInfoList([this](const ExitInfo &exitInfo, const std::vector<UserInfo> &list) {
        endAction(actionLoadUsers);
        if (exitInfo.code() != ExitCode::Ok) {
            notifyRequestFailure(exitInfo, RequestNum::USER_INFOLIST);
            return;
        }

        _appCache.replaceUsers(list);
    });
}

void UserService::loadAvailableDrives(const qint64 userDbId) {
    const auto requestedUserDbId = static_cast<UserDbId>(userDbId);
    beginAction(actionLoadAvailableDrives, static_cast<qint64>(requestedUserDbId));

    _commService.requestUserAvailableDrives(
            requestedUserDbId, [this, requestedUserDbId](const ExitInfo &exitInfo, const std::vector<DriveAvailableInfo> &list) {
                endAction(actionLoadAvailableDrives, static_cast<qint64>(requestedUserDbId));
                if (_appCache.selectedUserDbId() != static_cast<qint64>(requestedUserDbId)) {
                    return;
                }

                if (exitInfo.code() != ExitCode::Ok) {
                    notifyRequestFailure(exitInfo, RequestNum::USER_AVAILABLEDRIVES);
                    return;
                }

                _appCache.replaceAvailableDrives(list);
            });
}

void UserService::deleteUser(const qint64 userDbId) {
    beginAction(actionDeleteUser, userDbId);

    // Cache consistency is signal-driven: we wait for userRemoved/userUpdated pushes.
    _commService.requestDeleteUser(static_cast<UserDbId>(userDbId), [this, userDbId](const ExitInfo &exitInfo) {
        endAction(actionDeleteUser, userDbId);
        if (exitInfo.code() != ExitCode::Ok) {
            notifyRequestFailure(exitInfo, RequestNum::USER_DELETE);
        }
    });
}

void UserService::requestLoginToken(const QString &code, const QString &codeVerifier) {
    beginAction(actionRequestLoginToken);

    _commService.requestLoginToken(code, codeVerifier, [this](const ExitInfo &exitInfo, const LoginTokenResult &result) {
        endAction(actionRequestLoginToken);
        if (!result.error.isEmpty() || !result.errorDescription.isEmpty()) {
            _serviceEventBus.notifyGenericError(exitInfo, RequestNum::LOGIN_REQUESTTOKEN);
            emit loginTokenFailed(result.error, result.errorDescription);
            return;
        }

        if (exitInfo.code() != ExitCode::Ok) {
            notifyRequestFailure(exitInfo, RequestNum::LOGIN_REQUESTTOKEN);
            emit loginTokenFailed(QString(), QString());
            return;
        }

        emit loginTokenSucceeded(static_cast<qint64>(result.userDbId));
    });
}

bool UserService::isLoadUsersPending() const {
    return isActionPending(actionLoadUsers);
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

void UserService::beginAction(const QString &actionKey, const qint64 scopeId) {
    _serviceActionTracker.beginAction(serviceKeyUser, actionKey, scopeId);
}

void UserService::endAction(const QString &actionKey, const qint64 scopeId) {
    _serviceActionTracker.endAction(serviceKeyUser, actionKey, scopeId);
}

bool UserService::isActionPending(const QString &actionKey, const qint64 scopeId) const {
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
