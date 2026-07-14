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
 * Service objects use this class to expose durable UI-facing "pending" state without each service having to keep its own
 * loading counters. An action is identified by three keys:
 * - `serviceKey`: stable service identifier, for example "user", "drive", or "sync".
 * - `actionKey`: stable action identifier inside that service, for example "deleteUser" or "requestLoginToken".
 * - `scopeId`: optional discriminator for concurrent actions of the same type, for example a userDbId, driveDbId, or
 *   syncDbId.
 *
 * The default `scopeId` value, 0, is not a special "no scope" sentinel. It is a regular scope key stored in
 * `pendingCountByScope`, exactly like any other scope id. Calling `beginAction(service, action)` increments the counter
 * for scope 0, and calling `endAction(service, action)` decrements that same counter.
 *
 * Counts are reference-counted per `(serviceKey, actionKey, scopeId)`. This allows several identical requests to be
 * pending at the same time. `actionPendingChanged(..., true)` is emitted only when a scope counter transitions from 0 to
 * 1, and `actionPendingChanged(..., false)` is emitted only when that same scope counter returns to 0. Intermediate
 * increments and decrements do not emit action-level changes because the visible pending state did not change.
 * `endAllActions(...)` drains every pending occurrence for one exact `(serviceKey, actionKey, scopeId)` triplet and
 * emits the same state changes as the final matching `endAction(...)` call would emit.
 *
 * A service-level counter is also maintained across all actions and scopes for a given service. `servicePendingChanged`
 * follows the same transition rule: it is emitted when the first pending action starts for a service and when the last
 * pending action ends.
 *
 * Every `beginAction(...)` call must eventually be balanced either by one matching `endAction(...)` call or by an
 * `endAllActions(...)` call for the same `(serviceKey, actionKey, scopeId)`. Ending an unknown or non-pending action is
 * treated as a programming error: the tracker logs a warning and leaves its state unchanged.
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
        void endAllActions(const ServiceKey &serviceKey, const ActionKey &actionKey, ScopeId scopeId = 0);

    signals:
        void servicePendingChanged(const ServiceKey &serviceKey, bool pending);
        void actionPendingChanged(const ServiceKey &serviceKey, const ActionKey &actionKey, ScopeId scopeId, bool pending);

    private:
        enum class EndActionMode : uint8_t {
            One,
            All,
        };

        struct ActionPendingState {
                QHash<ScopeId, uint32_t> pendingCountByScope;
                uint32_t totalPendingCount{0};
        };

        void endActions(const ServiceKey &serviceKey, const ActionKey &actionKey, ScopeId scopeId, EndActionMode mode);

        QHash<ServiceKey, QHash<ActionKey, ActionPendingState>> _pendingByServiceAndAction;
        QHash<ServiceKey, uint32_t> _pendingCountByService;
};

} // namespace KDC
