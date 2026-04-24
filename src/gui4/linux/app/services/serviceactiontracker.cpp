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

#include "serviceactiontracker.h"

#include <QLoggingCategory>

namespace KDC {

Q_LOGGING_CATEGORY(lcServiceActionTracker, "gui.v4.serviceactiontracker", QtInfoMsg)

ServiceActionTracker::ServiceActionTracker(QObject *const parent) :
    QObject(parent) {}

bool ServiceActionTracker::isActionPending(const ServiceKey &serviceKey, const ActionKey &actionKey,
                                           const ScopeId scopeId) const {
    const auto serviceIt = _pendingByServiceAndAction.constFind(serviceKey);
    if (serviceIt == _pendingByServiceAndAction.constEnd()) {
        return false;
    }

    const auto actionIt = serviceIt->constFind(actionKey);
    if (actionIt == serviceIt->constEnd()) {
        return false;
    }

    const auto &actionState = actionIt.value();
    const auto scopeIt = actionState.pendingCountByScope.constFind(scopeId);
    if (scopeIt == actionState.pendingCountByScope.constEnd()) {
        return false;
    }

    return *scopeIt > 0;
}

bool ServiceActionTracker::isServicePending(const ServiceKey &serviceKey) const {
    return _pendingCountByService.value(serviceKey, 0) > 0;
}

void ServiceActionTracker::beginAction(const ServiceKey &serviceKey, const ActionKey &actionKey, const ScopeId scopeId) {
    auto &serviceActions = _pendingByServiceAndAction[serviceKey];
    auto &actionState = serviceActions[actionKey];
    const int32_t previousScopeCount = actionState.pendingCountByScope.value(scopeId, 0);
    const bool wasServicePending = isServicePending(serviceKey);

    actionState.pendingCountByScope[scopeId] = previousScopeCount + 1;
    actionState.totalPendingCount += 1;
    _pendingCountByService[serviceKey] = _pendingCountByService.value(serviceKey, 0) + 1;

    if (previousScopeCount == 0) {
        emit actionPendingChanged(serviceKey, actionKey, scopeId, true);
    }

    if (!wasServicePending) {
        emit servicePendingChanged(serviceKey, true);
    }
}

void ServiceActionTracker::endAction(const ServiceKey &serviceKey, const ActionKey &actionKey, const ScopeId scopeId) {
    auto serviceIt = _pendingByServiceAndAction.find(serviceKey);
    if (serviceIt == _pendingByServiceAndAction.end()) {
        qCWarning(lcServiceActionTracker) << "endAction called for unknown serviceKey:" << serviceKey
                                          << "| actionKey:" << actionKey << "| scopeId:" << scopeId;
        return;
    }

    auto &serviceActions = serviceIt.value();
    auto actionIt = serviceActions.find(actionKey);
    if (actionIt == serviceActions.end()) {
        qCWarning(lcServiceActionTracker) << "endAction called for unknown actionKey:" << actionKey
                                          << "| serviceKey:" << serviceKey << "| scopeId:" << scopeId;
        return;
    }

    auto &actionState = actionIt.value();
    auto scopeIt = actionState.pendingCountByScope.find(scopeId);
    if (scopeIt == actionState.pendingCountByScope.end() || scopeIt.value() <= 0) {
        qCWarning(lcServiceActionTracker) << "endAction called for non-pending scope | serviceKey:" << serviceKey
                                          << "| actionKey:" << actionKey << "| scopeId:" << scopeId;
        return;
    }

    scopeIt.value() -= 1;
    actionState.totalPendingCount -= 1;
    _pendingCountByService[serviceKey] -= 1;

    if (scopeIt.value() == 0) {
        actionState.pendingCountByScope.erase(scopeIt);
        emit actionPendingChanged(serviceKey, actionKey, scopeId, false);
    }

    if (actionState.totalPendingCount == 0) {
        serviceActions.erase(actionIt);
    }

    if (serviceActions.isEmpty()) {
        _pendingByServiceAndAction.erase(serviceIt);
    }

    if (_pendingCountByService.value(serviceKey, 0) <= 0) {
        _pendingCountByService.remove(serviceKey);
        emit servicePendingChanged(serviceKey, false);
    }
}

} // namespace KDC
