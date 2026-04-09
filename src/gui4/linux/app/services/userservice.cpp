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

#include <QPointer>

namespace KDC {

UserService::UserService(CommService &commService, AppCache &appCache, QObject *parent) :
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

    const QPointer<UserService> self(this);
    _commService.requestUserInfoList([self](const ExitInfo &exitInfo, const std::vector<UserInfo> &list) {
        if (!self) {
            return;
        }

        self->endRequest();
        if (exitInfo.code() != ExitCode::Ok) {
            self->setLastError(ServiceUtils::formatExitInfo(exitInfo));
            return;
        }

        self->_appCache.replaceUsers(list);
    });
}

void UserService::loadAvailableDrives(const qint64 userDbId) {
    beginRequest();
    setLastError(QString());

    const QPointer<UserService> self(this);
    _commService.requestUserAvailableDrives(static_cast<UserDbId>(userDbId),
                                            [self](const ExitInfo &exitInfo, const std::vector<DriveAvailableInfo> &list) {
                                                if (!self) {
                                                    return;
                                                }

                                                self->endRequest();
                                                if (exitInfo.code() != ExitCode::Ok) {
                                                    self->setLastError(ServiceUtils::formatExitInfo(exitInfo));
                                                    return;
                                                }

                                                self->_appCache.replaceAvailableDrives(list);
                                            });
}

void UserService::deleteUser(const qint64 userDbId) {
    beginRequest();
    setLastError(QString());

    const QPointer<UserService> self(this);
    // Cache consistency is signal-driven: we wait for userRemoved/userUpdated pushes.
    _commService.requestDeleteUser(static_cast<UserDbId>(userDbId), [self](const ExitInfo &exitInfo) {
        if (!self) {
            return;
        }

        self->endRequest();
        if (exitInfo.code() != ExitCode::Ok) {
            self->setLastError(ServiceUtils::formatExitInfo(exitInfo));
        }
    });
}

void UserService::requestLoginToken(const QString &code, const QString &codeVerifier) {
    beginRequest();
    setLastError(QString());

    const QPointer<UserService> self(this);
    _commService.requestLoginToken(code, codeVerifier, [self](const ExitInfo &exitInfo, const LoginTokenResult &result) {
        if (!self) {
            return;
        }

        self->endRequest();
        if (exitInfo.code() != ExitCode::Ok) {
            self->setLastError(ServiceUtils::formatExitInfo(exitInfo));
            emit self->loginTokenFailed(result.error, result.errorDescription);
            return;
        }

        emit self->loginTokenSucceeded(static_cast<qint64>(result.userDbId));
    });
}

void UserService::beginRequest() {
    ++_pendingRequestCount;
    setLoading(true);
}

void UserService::endRequest() {
    if (_pendingRequestCount <= 0) {
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
