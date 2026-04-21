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
#include "serviceutils.h"

#include <QLoggingCategory>

namespace KDC {

Q_LOGGING_CATEGORY(lcUserService, "gui.v4.userservice", QtInfoMsg)

UserService::UserService(CommService &commService, AppCache &appCache, QObject *const parent) :
    QObject(parent),
    _commService(commService),
    _appCache(appCache) {
    (void) connect(&_commService, &CommService::userAdded, &_appCache, &AppCache::upsertUser);
    (void) connect(&_commService, &CommService::userUpdated, &_appCache, &AppCache::upsertUser);
    (void) connect(&_commService, &CommService::userRemoved, &_appCache, &AppCache::removeUser);
}

void UserService::loadUsers() {
    beginRequest();
    setLastError(QString());

    _commService.requestUserInfoList([this](const ExitInfo &exitInfo, const std::vector<UserInfo> &list) {
        endRequest();
        if (exitInfo.code() != ExitCode::Ok) {
            setLastError(ServiceUtils::formatExitInfo(exitInfo));
            return;
        }

        _appCache.replaceUsers(list);
    });
}

void UserService::loadAvailableDrives(const qint64 userDbId) {
    beginRequest();
    setLastError(QString());

    const auto requestedUserDbId = static_cast<UserDbId>(userDbId);
    _commService.requestUserAvailableDrives(
            requestedUserDbId, [this, requestedUserDbId](const ExitInfo &exitInfo, const std::vector<DriveAvailableInfo> &list) {
                endRequest();
                if (_appCache.selectedUserDbId() != static_cast<qint64>(requestedUserDbId)) {
                    return;
                }

                if (exitInfo.code() != ExitCode::Ok) {
                    setLastError(ServiceUtils::formatExitInfo(exitInfo));
                    return;
                }

                _appCache.replaceAvailableDrives(list);
            });
}

void UserService::deleteUser(const qint64 userDbId) {
    beginRequest();
    setLastError(QString());

    // Cache consistency is signal-driven: we wait for userRemoved/userUpdated pushes.
    _commService.requestDeleteUser(static_cast<UserDbId>(userDbId), [this](const ExitInfo &exitInfo) {
        endRequest();
        if (exitInfo.code() != ExitCode::Ok) {
            setLastError(ServiceUtils::formatExitInfo(exitInfo));
        }
    });
}

void UserService::requestLoginToken(const QString &code, const QString &codeVerifier) {
    beginRequest();
    setLastError(QString());

    _commService.requestLoginToken(code, codeVerifier, [this](const ExitInfo &exitInfo, const LoginTokenResult &result) {
        endRequest();
        if (!result.error.isEmpty() || !result.errorDescription.isEmpty()) {
            emit loginTokenFailed(result.error, result.errorDescription);
            return;
        }

        if (exitInfo.code() != ExitCode::Ok) {
            const QString errorDescription = ServiceUtils::formatExitInfo(exitInfo);
            setLastError(errorDescription);
            emit loginTokenFailed(QString(), errorDescription);
            return;
        }

        emit loginTokenSucceeded(static_cast<qint64>(result.userDbId));
    });
}

void UserService::beginRequest() {
    ++_pendingRequestCount;
    setLoading(true);
}

void UserService::endRequest() {
    if (_pendingRequestCount <= 0) {
        qCWarning(lcUserService) << "endRequest called with non-positive pending count:" << _pendingRequestCount;
        _pendingRequestCount = 0;
        setLoading(false);
        return;
    }

    --_pendingRequestCount;
    if (_pendingRequestCount == 0) {
        setLoading(false);
    }
}

void UserService::setLoading(const bool loading) {
    if (_loading == loading) {
        return;
    }
    _loading = loading;
    emit loadingChanged();
}

void UserService::setLastError(const QString &error) {
    if (_lastError == error) {
        return;
    }
    _lastError = error;
    emit lastErrorChanged();
}

} // namespace KDC
