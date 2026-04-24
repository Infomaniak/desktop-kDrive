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

#include <QObject>
#include <QHash>
#include <QString>

#include <cstdint>

namespace KDC {

/**
 * Centralized state tracker for in-flight service actions.
 *
 * Role:
 * - Owns durable UI-facing "pending" state for service actions.
 * - Tracks pending status per (serviceKey, actionKey, scopeId) with reference-count semantics.
 * - Exposes pending lookup by action and by service scope.
 *
 * Contract:
 * - `serviceKey` must be a stable service identifier (example: "user", "drive", "sync").
 * - `actionKey` must be a stable action identifier local to a service (example: "deleteUser").
 * - `scopeId` can disambiguate concurrent actions of the same type (example: per userDbId).
 * - Every `beginAction(...)` call must be paired with exactly one `endAction(...)`.
 */
class ServiceActionTracker : public QObject {
        Q_OBJECT

    public:
        using ServiceKey = QString;
        using ActionKey = QString;
        using ScopeId = qint64;

        explicit ServiceActionTracker(QObject *parent = nullptr);

        Q_INVOKABLE [[nodiscard]] bool isActionPending(const ServiceKey &serviceKey, const ActionKey &actionKey,
                                                       ScopeId scopeId = 0) const;
        Q_INVOKABLE [[nodiscard]] bool isServicePending(const ServiceKey &serviceKey) const;
        void beginAction(const ServiceKey &serviceKey, const ActionKey &actionKey, ScopeId scopeId = 0);
        void endAction(const ServiceKey &serviceKey, const ActionKey &actionKey, ScopeId scopeId = 0);

    signals:
        void servicePendingChanged(const ServiceKey &serviceKey, bool pending);
        void actionPendingChanged(const ServiceKey &serviceKey, const ActionKey &actionKey, ScopeId scopeId, bool pending);

    private:
        struct ActionPendingState {
                QHash<ScopeId, int32_t> pendingCountByScope;
                int32_t totalPendingCount{0};
        };

        QHash<ServiceKey, QHash<ActionKey, ActionPendingState>> _pendingByServiceAndAction;
        QHash<ServiceKey, int32_t> _pendingCountByService;
};

} // namespace KDC
