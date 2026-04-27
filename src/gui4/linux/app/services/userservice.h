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
#include "app/services/serviceactiontracker.h"
#include "app/services/serviceeventbus.h"

#include <QObject>
#include <QString>

#include <cstdint>
#include <unordered_map>

namespace KDC {

/**
 * High-level user-oriented facade exposed to QML.
 *
 * Role:
 * - orchestrates user requests through CommService;
 * - updates AppCache snapshots and incremental entities;
 * - reports transient cross-service failures through ServiceEventBus;
 * - registers per-action pending state in ServiceActionTracker.
 */
class UserService : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)

    public:
        explicit UserService(CommService &commService, AppCache &appCache, ServiceActionTracker &serviceActionTracker,
                             ServiceEventBus &serviceEventBus, QObject *parent = nullptr);

        [[nodiscard]] bool loading() const { return _loading; }

        Q_INVOKABLE void loadUsers();
        Q_INVOKABLE void loadAvailableDrives(qint64 userDbId);
        Q_INVOKABLE void deleteUser(qint64 userDbId);
        Q_INVOKABLE void requestLoginToken(const QString &code, const QString &codeVerifier);
        Q_INVOKABLE [[nodiscard]] bool isLoadUsersPending() const;
        Q_INVOKABLE [[nodiscard]] bool isLoadAvailableDrivesPending(qint64 userDbId) const;
        Q_INVOKABLE [[nodiscard]] bool isDeleteUserPending(qint64 userDbId) const;
        Q_INVOKABLE [[nodiscard]] bool isLoginPending() const;

    signals:
        void loadingChanged();
        void loginTokenSucceeded(qint64 userDbId);
        void loginTokenFailed(const QString &error, const QString &errorDescription);

    private:
        void beginAction(const ServiceActionTracker::ActionKey &actionKey, ServiceActionTracker::ScopeId scopeId = 0);
        void endAction(const ServiceActionTracker::ActionKey &actionKey, ServiceActionTracker::ScopeId scopeId = 0);
        void setLoading(bool loading);
        [[nodiscard]] bool isActionPending(const ServiceActionTracker::ActionKey &actionKey,
                                           ServiceActionTracker::ScopeId scopeId = 0) const;
        void notifyRequestFailure(const ExitInfo &exitInfo, RequestNum requestNum);

        CommService &_commService;
        AppCache &_appCache;
        ServiceActionTracker &_serviceActionTracker;
        ServiceEventBus &_serviceEventBus;
        std::unordered_map<UserDbId, uint64_t> _availableDriveLoadGenerations;
        bool _loading{false};
};

} // namespace KDC
