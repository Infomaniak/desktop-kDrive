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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "networkstatusobserver.h"

#include <QLoggingCategory>
#include <QNetworkInformation>

namespace KDC {

namespace {
Q_LOGGING_CATEGORY(lcNetworkStatusObserver, "gui.v4.networkstatus", QtInfoMsg)

bool isOfflineReachability(const QNetworkInformation::Reachability reachability) {
    switch (reachability) {
        case QNetworkInformation::Reachability::Disconnected:
            return true;
        case QNetworkInformation::Reachability::Unknown:
        case QNetworkInformation::Reachability::Local:
        case QNetworkInformation::Reachability::Site:
        case QNetworkInformation::Reachability::Online:
            return false;
    }
    return false;
}
} // namespace

NetworkStatusObserver::NetworkStatusObserver(QObject *const parent) :
    QObject(parent) {
    // If Flatpak, this class requires either D-Bus access to org.freedesktop.NetworkManager or the Qt GLib backend using the
    // NetworkMonitor portal.
    if (!QNetworkInformation::loadDefaultBackend()) {
        qCWarning(lcNetworkStatusObserver) << "No Qt network information backend available; offline detection disabled";
        return;
    }

    const auto *const networkInformation = QNetworkInformation::instance();
    if (networkInformation == nullptr) {
        qCWarning(lcNetworkStatusObserver) << "Qt network information backend loaded without an instance";
        return;
    }

    qCInfo(lcNetworkStatusObserver) << "Network status backend loaded | backend:" << networkInformation->backendName()
                                    << "/ available backends:" << QNetworkInformation::availableBackends();
    (void) connect(networkInformation, &QNetworkInformation::reachabilityChanged, this,
                   &NetworkStatusObserver::updateReachability);
    updateReachability();
}

void NetworkStatusObserver::updateReachability() {
    const auto *const networkInformation = QNetworkInformation::instance();
    if (networkInformation == nullptr) {
        return;
    }

    const auto reachability = networkInformation->reachability();
    const bool offline = isOfflineReachability(reachability);
    qCInfo(lcNetworkStatusObserver) << "Network reachability evaluated | backend:" << networkInformation->backendName()
                                    << "/ reachability:" << reachability << "/ offline:" << offline;
    if (_offline == offline) {
        return;
    }

    _offline = offline;
    emit offlineChanged();
}

} // namespace KDC
