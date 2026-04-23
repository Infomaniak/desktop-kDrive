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

#pragma once

#include "app/cache/appcache.h"
#include "app/services/commservice.h"
#include "app/services/serviceeventbus.h"

#include <QObject>
#include <QString>

#include <cstdint>

namespace KDC {

/**
 * High-level user-oriented facade exposed to QML.
 *
 * This service orchestrates user requests over CommService and updates AppCache.
 */
class UserService : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)

    public:
        explicit UserService(CommService &commService, AppCache &appCache, ServiceEventBus &serviceEventBus,
                             QObject *parent = nullptr);

        bool loading() const { return _loading; }

        Q_INVOKABLE void loadUsers();
        Q_INVOKABLE void loadAvailableDrives(qint64 userDbId);
        Q_INVOKABLE void deleteUser(qint64 userDbId);
        Q_INVOKABLE void requestLoginToken(const QString &code, const QString &codeVerifier);

    signals:
        void loadingChanged();
        void loginTokenSucceeded(qint64 userDbId);
        void loginTokenFailed(const QString &error, const QString &errorDescription);

    private:
        void beginRequest();
        void endRequest();
        void setLoading(bool loading);
        void notifyRequestFailure(const ExitInfo &exitInfo, RequestNum requestNum);

        CommService &_commService;
        AppCache &_appCache;
        ServiceEventBus &_serviceEventBus;
        int32_t _pendingRequestCount{0};
        bool _loading{false};
};

} // namespace KDC
